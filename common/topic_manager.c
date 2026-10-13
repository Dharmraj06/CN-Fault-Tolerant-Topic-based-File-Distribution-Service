/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/topic_manager.c
 * Description: Topic Discovery, Directory Traversal Security, and Manifest Implementation.
 *
 * Phase: Phase 1 (Core Wire Framing & Baseline Server)
 * Status: FROZEN v1.0
 *
 * Source of Truth Hierarchy:
 *   - Tier 1: IT305 Assignment Specification (§Part 1, §DATASET)
 *   - Tier 2: Instructor Clarifications (§Page 1)
 *   - Tier 3: Approved Design Document Group 071_072 (§5.2, §9.1, §21)
 *   - Tier 4: Master Execution Roadmap (PART2_TASKS_ROADMAP.md §4.2.2, §5 Task 1.2)
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "topic_manager.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

/* ----------------------------------------------------------------------------
 * IEEE 802.3 CRC32 Implementation
 * Polynomial: 0xEDB88320 (Reversed representation)
 * ---------------------------------------------------------------------------- */

static uint32_t s_crc32_table[256];
static bool s_crc32_initialized = false;

static void init_crc32_table(void) {
    if (s_crc32_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        s_crc32_table[i] = c;
    }
    s_crc32_initialized = true;
}

static uint32_t crc32_update(uint32_t crc, const void *data, size_t len) {
    if (!s_crc32_initialized) {
        init_crc32_table();
    }
    const uint8_t *p = (const uint8_t *)data;
    while (len--) {
        crc = s_crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

uint32_t compute_manifest_crc32(const FileEntry *files, uint32_t file_count) {
    if (!files || file_count == 0) {
        return 0;
    }

    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < file_count; i++) {
        /*
         * Hash the filename string including its trailing null terminator.
         * Including the null delimiter prevents hash ambiguity between
         * adjacent string boundaries.
         */
        size_t name_len = strlen(files[i].filename) + 1;
        crc = crc32_update(crc, files[i].filename, name_len);

        /*
         * Hash the 8-byte file size serialized in Big-Endian network byte order.
         * This guarantees identical fingerprints across x86, ARM, and Big-Endian CPUs.
         */
        uint64_t net_size = proto_hton64(files[i].file_size);
        crc = crc32_update(crc, &net_size, sizeof(net_size));
    }

    return crc ^ 0xFFFFFFFF;
}

/* ----------------------------------------------------------------------------
 * Topic Name Sanitization & Validation
 * ---------------------------------------------------------------------------- */

TopicError validate_topic_name(const char *topic) {
    if (topic == NULL) {
        return TOPIC_ERR_INVALID_NAME;
    }

    size_t len = strlen(topic);
    if (len == 0 || len >= MAX_TOPIC_NAME_LEN) {
        return TOPIC_ERR_INVALID_NAME;
    }

    /* Reject hidden directories / leading dots (e.g. ".", "..", ".git") */
    if (topic[0] == '.') {
        return TOPIC_ERR_INVALID_NAME;
    }

    /* Reject path traversal sequence ".." anywhere */
    if (strstr(topic, "..") != NULL) {
        return TOPIC_ERR_INVALID_NAME;
    }

    /* Reject Windows drive letters (e.g. "C:", "D:") */
    if (len >= 2 && topic[1] == ':' && ((topic[0] >= 'a' && topic[0] <= 'z') || (topic[0] >= 'A' && topic[0] <= 'Z'))) {
        return TOPIC_ERR_INVALID_NAME;
    }

    /* Scan every character for path separators and non-whitelisted characters */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)topic[i];

        /* Reject non-printable ASCII or DEL */
        if (c < 32 || c == 127) {
            return TOPIC_ERR_INVALID_NAME;
        }

        /* Reject Unix and Windows path separators */
        if (c == '/' || c == '\\') {
            return TOPIC_ERR_INVALID_NAME;
        }

        /* Reject shell injection and forbidden filesystem metacharacters */
        if (c == '*' || c == '?' || c == '"' || c == '\'' ||
            c == '<' || c == '>' || c == '|' || c == ':' ||
            c == ';' || c == '&' || c == '$' || c == '`') {
            return TOPIC_ERR_INVALID_NAME;
        }
    }

    return TOPIC_OK;
}

/* ----------------------------------------------------------------------------
 * Safe Path Construction
 * ---------------------------------------------------------------------------- */

TopicError build_topic_path(const char *root_dir, const char *topic, char *out_path, size_t max_len) {
    if (root_dir == NULL || out_path == NULL || max_len == 0) {
        return TOPIC_ERR_INVALID_NAME;
    }

    TopicError err = validate_topic_name(topic);
    if (err != TOPIC_OK) {
        return err;
    }

    /* Strip trailing slashes from root_dir for clean formatting */
    size_t root_len = strlen(root_dir);
    while (root_len > 1 && (root_dir[root_len - 1] == '/' || root_dir[root_len - 1] == '\\')) {
        root_len--;
    }

    int n = snprintf(out_path, max_len, "%.*s/%s", (int)root_len, root_dir, topic);
    if (n < 0 || (size_t)n >= max_len) {
        return TOPIC_ERR_PATH_TOO_LONG;
    }

    return TOPIC_OK;
}

/* ----------------------------------------------------------------------------
 * Lexicographical Sorting Helper
 * ---------------------------------------------------------------------------- */

static int compare_file_entries(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    return strcmp(fa->filename, fb->filename);
}

/* ----------------------------------------------------------------------------
 * Topic Manifest Builder
 * ---------------------------------------------------------------------------- */

TopicError build_topic_manifest(const char *root_dir, const char *topic, TopicManifest **out_manifest) {
    if (root_dir == NULL || topic == NULL || out_manifest == NULL) {
        return TOPIC_ERR_INVALID_NAME;
    }
    *out_manifest = NULL;

    /* Step 1: Validate topic name */
    TopicError val_err = validate_topic_name(topic);
    if (val_err != TOPIC_OK) {
        return val_err;
    }

    /* Step 2: Canonicalize root directory to resolve symlinks and relative paths */
    char canonical_root[PATH_MAX];
    if (realpath(root_dir, canonical_root) == NULL) {
        if (errno == ENOENT) return TOPIC_ERR_NOT_FOUND;
        if (errno == EACCES) return TOPIC_ERR_PERMISSION;
        return TOPIC_ERR_IO;
    }

    struct stat root_st;
    if (stat(canonical_root, &root_st) != 0 || !S_ISDIR(root_st.st_mode)) {
        return TOPIC_ERR_NOT_DIR;
    }

    /* Step 3: Safely construct topic directory path */
    char raw_topic_path[PATH_MAX];
    TopicError path_err = build_topic_path(canonical_root, topic, raw_topic_path, sizeof(raw_topic_path));
    if (path_err != TOPIC_OK) {
        return path_err;
    }

    /*
     * Step 4: Defense-in-depth symlink escape check.
     * Use lstat() on the topic directory itself: if it is a symbolic link,
     * immediately reject it to prevent a symlink from escaping the topic root.
     */
    struct stat topic_lstat;
    if (lstat(raw_topic_path, &topic_lstat) != 0) {
        if (errno == ENOENT) return TOPIC_ERR_NOT_FOUND;
        if (errno == EACCES) return TOPIC_ERR_PERMISSION;
        return TOPIC_ERR_IO;
    }
    if (S_ISLNK(topic_lstat.st_mode)) {
        /* Symlink directory detected: reject escape attempt */
        return TOPIC_ERR_ESCAPE;
    }
    if (!S_ISDIR(topic_lstat.st_mode)) {
        return TOPIC_ERR_NOT_DIR;
    }

    /* Step 5: Canonicalize topic path and verify it stays inside canonical_root */
    char canonical_topic[PATH_MAX];
    if (realpath(raw_topic_path, canonical_topic) == NULL) {
        if (errno == ENOENT) return TOPIC_ERR_NOT_FOUND;
        if (errno == EACCES) return TOPIC_ERR_PERMISSION;
        return TOPIC_ERR_IO;
    }

    size_t root_len = strlen(canonical_root);
    if (strncmp(canonical_topic, canonical_root, root_len) != 0 ||
        (canonical_topic[root_len] != '/' && canonical_topic[root_len] != '\0')) {
        /* Path resolved outside root: security violation */
        return TOPIC_ERR_ESCAPE;
    }

    /* Step 6: Open directory using POSIX opendir */
    DIR *dir = opendir(canonical_topic);
    if (dir == NULL) {
        if (errno == ENOENT) return TOPIC_ERR_NOT_FOUND;
        if (errno == EACCES) return TOPIC_ERR_PERMISSION;
        if (errno == ENOTDIR) return TOPIC_ERR_NOT_DIR;
        return TOPIC_ERR_IO;
    }

    int dfd = dirfd(dir);
    if (dfd < 0) {
        closedir(dir);
        return TOPIC_ERR_IO;
    }

    /* Step 7: Enumerate entries and filter strictly for regular files */
    size_t capacity = 32;
    FileEntry *file_list = malloc(capacity * sizeof(FileEntry));
    if (file_list == NULL) {
        closedir(dir);
        return TOPIC_ERR_NO_MEMORY;
    }

    uint32_t count = 0;
    uint64_t total_bytes = 0;
    struct dirent *entry = NULL;

    while ((entry = readdir(dir)) != NULL) {
        /* Ignore hidden files and directory links (".", "..", ".git", etc.) */
        if (entry->d_name[0] == '.') {
            continue;
        }

        /* Check filename length boundary */
        if (strlen(entry->d_name) >= MAX_FILENAME_LEN) {
            continue; /* Skip names exceeding maximum filename limit */
        }

        /*
         * Use fstatat() with AT_SYMLINK_NOFOLLOW via dirfd.
         * This avoids string path concatenation and TOCTOU symlink races.
         * If the file is a symlink, S_ISLNK is true and S_ISREG is false,
         * ensuring symlinks to sensitive files are strictly excluded!
         */
        struct stat st;
        if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
            continue; /* Unreadable or dangling entry */
        }

        if (!S_ISREG(st.st_mode)) {
            /* Reject subdirectories, symlinks, fifos, sockets, and devices */
            continue;
        }

        /* Grow array if needed */
        if (count >= capacity) {
            size_t new_cap = capacity * 2;
            FileEntry *new_list = realloc(file_list, new_cap * sizeof(FileEntry));
            if (new_list == NULL) {
                free(file_list);
                closedir(dir);
                return TOPIC_ERR_NO_MEMORY;
            }
            file_list = new_list;
            capacity = new_cap;
        }

        /* Populate FileEntry */
        strncpy(file_list[count].filename, entry->d_name, MAX_FILENAME_LEN - 1);
        file_list[count].filename[MAX_FILENAME_LEN - 1] = '\0';

        snprintf(file_list[count].relative_path, sizeof(file_list[count].relative_path),
                 "%s/%s", topic, entry->d_name);

        file_list[count].file_size = (uint64_t)st.st_size;
        total_bytes += (uint64_t)st.st_size;
        count++;
    }

    closedir(dir);

    /* Step 8: Deterministic lexicographical sort */
    if (count > 0) {
        qsort(file_list, count, sizeof(FileEntry), compare_file_entries);
    } else {
        /* Empty topic directory: free array and set to NULL */
        free(file_list);
        file_list = NULL;
    }

    /* Step 9: Compute 32-bit CRC32 manifest fingerprint */
    uint32_t fingerprint = compute_manifest_crc32(file_list, count);

    /* Step 10: Allocate and populate TopicManifest container */
    TopicManifest *manifest = calloc(1, sizeof(TopicManifest));
    if (manifest == NULL) {
        free(file_list);
        return TOPIC_ERR_NO_MEMORY;
    }

    strncpy(manifest->topic_name, topic, MAX_TOPIC_NAME_LEN - 1);
    manifest->topic_name[MAX_TOPIC_NAME_LEN - 1] = '\0';

    strncpy(manifest->directory_path, canonical_topic, MAX_PATH_BUFFER_LEN - 1);
    manifest->directory_path[MAX_PATH_BUFFER_LEN - 1] = '\0';

    manifest->file_count = count;
    manifest->total_topic_bytes = total_bytes;
    manifest->manifest_fingerprint = fingerprint;
    manifest->files = file_list;

    *out_manifest = manifest;
    return TOPIC_OK;
}

/* ----------------------------------------------------------------------------
 * Deallocation
 * ---------------------------------------------------------------------------- */

void free_topic_manifest(TopicManifest *manifest) {
    if (manifest == NULL) {
        return;
    }
    if (manifest->files != NULL) {
        free(manifest->files);
        manifest->files = NULL;
    }
    free(manifest);
}

/* ----------------------------------------------------------------------------
 * Manifest Inspection Helpers
 * ---------------------------------------------------------------------------- */

const FileEntry* topic_manifest_get_file(const TopicManifest *manifest, uint32_t file_index) {
    if (manifest == NULL || manifest->files == NULL) {
        return NULL;
    }
    if (file_index >= manifest->file_count) {
        return NULL;
    }
    return &manifest->files[file_index];
}

const FileEntry* topic_manifest_find_file(const TopicManifest *manifest, const char *filename) {
    if (manifest == NULL || manifest->files == NULL || filename == NULL) {
        return NULL;
    }

    FileEntry key;
    strncpy(key.filename, filename, sizeof(key.filename) - 1);
    key.filename[sizeof(key.filename) - 1] = '\0';

    return (const FileEntry *)bsearch(&key, manifest->files, manifest->file_count,
                                      sizeof(FileEntry), compare_file_entries);
}

const char* topic_error_to_string(TopicError err) {
    switch (err) {
        case TOPIC_OK:
            return "Success";
        case TOPIC_ERR_INVALID_NAME:
            return "Invalid topic name (path traversal or forbidden characters)";
        case TOPIC_ERR_NOT_FOUND:
            return "Topic directory not found";
        case TOPIC_ERR_NOT_DIR:
            return "Target path is not a directory";
        case TOPIC_ERR_PERMISSION:
            return "Permission denied reading topic directory";
        case TOPIC_ERR_ESCAPE:
            return "Path or symbolic link escapes configured topics root";
        case TOPIC_ERR_NO_MEMORY:
            return "Memory allocation failure";
        case TOPIC_ERR_IO:
            return "Filesystem I/O error";
        case TOPIC_ERR_PATH_TOO_LONG:
            return "Topic path exceeds buffer capacity";
        default:
            return "Unknown topic error";
    }
}
