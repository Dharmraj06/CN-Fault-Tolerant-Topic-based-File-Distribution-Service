/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/topic_manager.h
 * Description: Topic Discovery, Directory Traversal Security, and Manifest Engine.
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

#ifndef TOPIC_MANAGER_H
#define TOPIC_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------------
 * Topic & Path Limits
 * ---------------------------------------------------------------------------- */
#define MAX_TOPIC_NAME_LEN    128   /* Maximum length of topic name (including null terminator) */
#define MAX_FILENAME_LEN      256   /* Maximum filename length (including null terminator) */
#define MAX_PATH_BUFFER_LEN   1024  /* Maximum filesystem path length */

/* ----------------------------------------------------------------------------
 * Topic Manager Error Codes
 * ---------------------------------------------------------------------------- */
typedef enum {
    TOPIC_OK                  =  0, /* Operation succeeded */
    TOPIC_ERR_INVALID_NAME    = -1, /* Name is empty, contains path traversal, or forbidden chars */
    TOPIC_ERR_NOT_FOUND       = -2, /* Topic directory does not exist */
    TOPIC_ERR_NOT_DIR         = -3, /* Path exists but is not a directory */
    TOPIC_ERR_PERMISSION      = -4, /* Permission denied reading directory or files */
    TOPIC_ERR_ESCAPE          = -5, /* Symlink or resolved path escapes configured topics root */
    TOPIC_ERR_NO_MEMORY       = -6, /* Dynamic memory allocation failure */
    TOPIC_ERR_IO              = -7, /* Filesystem read error during directory enumeration */
    TOPIC_ERR_PATH_TOO_LONG   = -8  /* Path length exceeds buffer capacity */
} TopicError;

/* ----------------------------------------------------------------------------
 * Manifest Data Structures
 * ---------------------------------------------------------------------------- */

/**
 * FileEntry:
 * Metadata describing a single regular file belonging to a topic.
 */
typedef struct {
    char filename[MAX_FILENAME_LEN];      /* Basename of file (e.g. "image_0001.jpg") */
    char relative_path[MAX_PATH_BUFFER_LEN]; /* Relative path from root (e.g. "cane/image_0001.jpg") */
    uint64_t file_size;                   /* Total file size in bytes (from stat) */
} FileEntry;

/**
 * TopicManifest:
 * Complete topic manifest containing the sorted list of regular files,
 * total byte count, and a deterministic 32-bit CRC32 fingerprint.
 *
 * A topic manifest is built once when a topic is requested.
 * Sorting filenames lexicographically via qsort() guarantees that all
 * worker threads, resumed sessions, and clients process files in the exact
 * same deterministic sequence regardless of filesystem readdir() order.
 */
typedef struct {
    char topic_name[MAX_TOPIC_NAME_LEN];        /* Sanitized topic name */
    char directory_path[MAX_PATH_BUFFER_LEN];   /* Canonical absolute directory path */
    uint32_t file_count;                        /* Number of regular files in topic */
    uint64_t total_topic_bytes;                 /* Sum of all file sizes in bytes */
    uint32_t manifest_fingerprint;              /* 32-bit IEEE 802.3 CRC32 digest */
    FileEntry *files;                           /* Dynamically allocated array of sorted files */
} TopicManifest;

/* ----------------------------------------------------------------------------
 * Security & Validation Interface
 * ---------------------------------------------------------------------------- */

/**
 * validate_topic_name:
 * Validates a client-supplied topic name against directory traversal and
 * injection attacks.
 *
 * Security checks:
 *   - Rejects NULL or empty strings.
 *   - Rejects names >= MAX_TOPIC_NAME_LEN bytes.
 *   - Rejects path traversal sequences ("..").
 *   - Rejects path separators ('/' and '\\').
 *   - Rejects absolute paths (leading '/' or Windows drive specifiers "C:").
 *   - Rejects hidden directories (names starting with '.').
 *   - Rejects non-printable ASCII (< 32, 127) and shell metacharacters (*, ?, ", <, >, |, :, ;, &, $, `).
 *
 * Parameters:
 *   topic : Client-supplied topic string.
 *
 * Return value:
 *   TOPIC_OK               : Name is safe and well-formed.
 *   TOPIC_ERR_INVALID_NAME : Malformed name or security violation attempt.
 */
TopicError validate_topic_name(const char *topic);

/**
 * build_topic_path:
 * Safely constructs `<root_dir>/<topic>` in `out_path` after validation.
 *
 * Parameters:
 *   root_dir : Server's configured topics root directory.
 *   topic    : Validated topic name.
 *   out_path : Destination buffer to receive constructed path.
 *   max_len  : Size of out_path buffer.
 *
 * Return value:
 *   TOPIC_OK                : Path safely constructed.
 *   TOPIC_ERR_INVALID_NAME  : Topic name failed validation.
 *   TOPIC_ERR_PATH_TOO_LONG : Path exceeds max_len.
 */
TopicError build_topic_path(const char *root_dir, const char *topic, char *out_path, size_t max_len);

/* ----------------------------------------------------------------------------
 * Manifest Construction & Deallocation Interface
 * ---------------------------------------------------------------------------- */

/**
 * build_topic_manifest:
 * Scans `<root_dir>/<topic>`, verifies canonical confinement within `root_dir`,
 * enumerates all regular files (strictly rejecting symlinks, subdirs, and pipes),
 * sorts filenames in deterministic ASCII order, and calculates the 32-bit CRC32
 * manifest fingerprint.
 *
 * Parameters:
 *   root_dir     : Configured topics root directory path.
 *   topic        : Client-requested topic name.
 *   out_manifest : Address of pointer to receive the allocated TopicManifest.
 *
 * Ownership Contract:
 *   Upon success, the caller OWNS the allocated TopicManifest and MUST
 *   free it using free_topic_manifest(). On error, *out_manifest is set to NULL.
 *
 * Return value:
 *   TOPIC_OK                : Manifest successfully built.
 *   TOPIC_ERR_INVALID_NAME  : Topic name failed validation.
 *   TOPIC_ERR_NOT_FOUND     : Topic directory does not exist.
 *   TOPIC_ERR_NOT_DIR       : Path is not a directory.
 *   TOPIC_ERR_PERMISSION    : Directory read permission denied.
 *   TOPIC_ERR_ESCAPE        : Path or symlink escapes configured topics root.
 *   TOPIC_ERR_NO_MEMORY     : Memory allocation failed.
 *   TOPIC_ERR_IO            : Filesystem reading error.
 */
TopicError build_topic_manifest(const char *root_dir, const char *topic, TopicManifest **out_manifest);

/**
 * free_topic_manifest:
 * Safely deallocates a TopicManifest and its internal FileEntry array.
 * Safe to call with NULL.
 *
 * Parameters:
 *   manifest : Pointer to TopicManifest to free.
 */
void free_topic_manifest(TopicManifest *manifest);

/* ----------------------------------------------------------------------------
 * Manifest Inspection Helpers
 * ---------------------------------------------------------------------------- */

/**
 * topic_manifest_get_file:
 * Retrieves a pointer to the FileEntry at index `file_index`.
 *
 * Return value:
 *   Pointer to FileEntry if file_index < manifest->file_count; NULL otherwise.
 */
const FileEntry* topic_manifest_get_file(const TopicManifest *manifest, uint32_t file_index);

/**
 * topic_manifest_find_file:
 * Searches the sorted manifest for a file named `filename` using binary search.
 *
 * Return value:
 *   Pointer to FileEntry if found; NULL if not found.
 */
const FileEntry* topic_manifest_find_file(const TopicManifest *manifest, const char *filename);

/**
 * compute_manifest_crc32:
 * Calculates IEEE 802.3 CRC32 checksum over the sorted files and sizes.
 *
 * Representation:
 *   For each file in sorted order:
 *     - Filename bytes including trailing '\0'
 *     - 8-byte file_size in big-endian network byte order
 */
uint32_t compute_manifest_crc32(const FileEntry *files, uint32_t file_count);

/**
 * topic_error_to_string:
 * Translates a TopicError code into a human-readable diagnostic message.
 */
const char* topic_error_to_string(TopicError err);

#ifdef __cplusplus
}
#endif

#endif /* TOPIC_MANAGER_H */
