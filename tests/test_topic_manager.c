/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: tests/test_topic_manager.c
 * Description: Comprehensive Unit & Security Test Suite for common/topic_manager.*
 *
 * Author: Jay Vaghela (Student ID: 202401237)
 * Phase: Phase 1 (Task 1.2 Topic Security & Manifest Verification)
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "topic_manager.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TEST_PASS(name) printf("  [PASS] %s\n", name)
#define TEST_FAIL(name, msg) do { \
    fprintf(stderr, "  [FAIL] %s: %s (errno=%d: %s)\n", name, msg, errno, strerror(errno)); \
    exit(1); \
} while (0)

static int total_tests = 0;
static int passed_tests = 0;

static void run_test(const char *name, void (*test_func)(void)) {
    total_tests++;
    printf("[RUNNING] Test %d: %s\n", total_tests, name);
    test_func();
    passed_tests++;
    TEST_PASS(name);
}

static char test_root_dir[256];

/* Helper to write dummy file */
static void create_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Failed to create file: %s (errno=%d)\n", path, errno);
        exit(1);
    }
    if (content && strlen(content) > 0) {
        size_t written = fwrite(content, 1, strlen(content), f);
        (void)written;
    }
    fclose(f);
}

/* Setup test directory fixture tree */
static void setup_test_fixtures(void) {
    snprintf(test_root_dir, sizeof(test_root_dir), "/tmp/test_topics_%d", (int)getpid());

    /* Remove any stale dir */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s && mkdir -p %s", test_root_dir, test_root_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to create test root: %s\n", test_root_dir);
        exit(1);
    }

    /* 1. valid_topic */
    char path[1024];
    snprintf(path, sizeof(path), "%s/valid_topic", test_root_dir);
    mkdir(path, 0755);

    /* Create files out of alphabetical order */
    snprintf(path, sizeof(path), "%s/valid_topic/file_c.txt", test_root_dir);
    create_file(path, "1234567890"); /* 10 bytes */

    snprintf(path, sizeof(path), "%s/valid_topic/file_a.txt", test_root_dir);
    create_file(path, "1234567890123456789012345"); /* 25 bytes */

    snprintf(path, sizeof(path), "%s/valid_topic/file_b.txt", test_root_dir);
    char buf100[101];
    memset(buf100, 'B', 100);
    buf100[100] = '\0';
    create_file(path, buf100); /* 100 bytes */

    /* 2. empty_topic */
    snprintf(path, sizeof(path), "%s/empty_topic", test_root_dir);
    mkdir(path, 0755);

    /* 3. filtered_topic (contains subdir, symlink, hidden file, fifo) */
    snprintf(path, sizeof(path), "%s/filtered_topic", test_root_dir);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), "%s/filtered_topic/regular.bin", test_root_dir);
    create_file(path, "Hello Regular File"); /* 18 bytes */

    /* Subdirectory */
    snprintf(path, sizeof(path), "%s/filtered_topic/sub_dir", test_root_dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/filtered_topic/sub_dir/inside.txt", test_root_dir);
    create_file(path, "Hidden in subdir");

    /* Hidden file */
    snprintf(path, sizeof(path), "%s/filtered_topic/.hidden_data", test_root_dir);
    create_file(path, "Secrets");

    /* Symlink to regular file */
    snprintf(path, sizeof(path), "%s/filtered_topic/symlink_link", test_root_dir);
    symlink("regular.bin", path);

    /* FIFO pipe */
    snprintf(path, sizeof(path), "%s/filtered_topic/test_fifo", test_root_dir);
    mkfifo(path, 0644);

    /* 4. symlink_topic (Topic folder that is itself a symlink to outside directory) */
    snprintf(path, sizeof(path), "%s/symlink_escape_topic", test_root_dir);
    symlink("/tmp", path);
}

static void teardown_test_fixtures(void) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_root_dir);
    (void)system(cmd);
}

/* ----------------------------------------------------------------------------
 * Test 1: validate_topic_name Security Checks
 * ---------------------------------------------------------------------------- */
static void test_validate_topic_name(void) {
    /* Valid topics */
    assert(validate_topic_name("cane") == TOPIC_OK);
    assert(validate_topic_name("cavallo") == TOPIC_OK);
    assert(validate_topic_name("topic_123") == TOPIC_OK);
    assert(validate_topic_name("Animal-Dataset") == TOPIC_OK);

    /* Path traversal attempts */
    assert(validate_topic_name("..") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name(".") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("../../etc/passwd") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("cane/../cavallo") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("topic/child") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("topic\\child") == TOPIC_ERR_INVALID_NAME);

    /* Absolute paths */
    assert(validate_topic_name("/etc/passwd") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("/var/log") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("C:\\Windows") == TOPIC_ERR_INVALID_NAME);

    /* Hidden names */
    assert(validate_topic_name(".hidden") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name(".git") == TOPIC_ERR_INVALID_NAME);

    /* Metacharacters / Injection */
    assert(validate_topic_name("topic;rm") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("topic*") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("topic?") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("topic|pipe") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("topic&bg") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("topic$var") == TOPIC_ERR_INVALID_NAME);

    /* Null / Empty / Length boundaries */
    assert(validate_topic_name(NULL) == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("") == TOPIC_ERR_INVALID_NAME);

    char giant[200];
    memset(giant, 'A', sizeof(giant));
    giant[199] = '\0';
    assert(validate_topic_name(giant) == TOPIC_ERR_INVALID_NAME);
}

/* ----------------------------------------------------------------------------
 * Test 2: build_topic_path Formatting
 * ---------------------------------------------------------------------------- */
static void test_build_topic_path(void) {
    char out_path[512];
    TopicError err = build_topic_path("/var/topics/", "cane", out_path, sizeof(out_path));
    assert(err == TOPIC_OK);
    assert(strcmp(out_path, "/var/topics/cane") == 0);

    /* Without trailing slash */
    err = build_topic_path("/var/topics", "cane", out_path, sizeof(out_path));
    assert(err == TOPIC_OK);
    assert(strcmp(out_path, "/var/topics/cane") == 0);

    /* Traversal rejected */
    err = build_topic_path("/var/topics", "../etc", out_path, sizeof(out_path));
    assert(err == TOPIC_ERR_INVALID_NAME);

    /* Buffer capacity exhaustion */
    char tiny[10];
    err = build_topic_path("/var/topics", "cane", tiny, sizeof(tiny));
    assert(err == TOPIC_ERR_PATH_TOO_LONG);
}

/* ----------------------------------------------------------------------------
 * Test 3: Valid Topic Enumeration, Sorting, and CRC32
 * ---------------------------------------------------------------------------- */
static void test_valid_topic_manifest(void) {
    TopicManifest *manifest = NULL;
    TopicError err = build_topic_manifest(test_root_dir, "valid_topic", &manifest);
    assert(err == TOPIC_OK);
    assert(manifest != NULL);

    assert(strcmp(manifest->topic_name, "valid_topic") == 0);
    assert(manifest->file_count == 3);
    assert(manifest->total_topic_bytes == (10 + 25 + 100)); /* 135 bytes */

    /* Verify lexicographical sort: file_a.txt < file_b.txt < file_c.txt */
    assert(strcmp(manifest->files[0].filename, "file_a.txt") == 0);
    assert(manifest->files[0].file_size == 25);
    assert(strcmp(manifest->files[0].relative_path, "valid_topic/file_a.txt") == 0);

    assert(strcmp(manifest->files[1].filename, "file_b.txt") == 0);
    assert(manifest->files[1].file_size == 100);

    assert(strcmp(manifest->files[2].filename, "file_c.txt") == 0);
    assert(manifest->files[2].file_size == 10);

    /* Verify manifest fingerprint is non-zero and reproducible */
    assert(manifest->manifest_fingerprint != 0);
    uint32_t fp_check = compute_manifest_crc32(manifest->files, manifest->file_count);
    assert(manifest->manifest_fingerprint == fp_check);

    /* Test helper lookups */
    const FileEntry *fe = topic_manifest_get_file(manifest, 1);
    assert(fe != NULL);
    assert(strcmp(fe->filename, "file_b.txt") == 0);

    assert(topic_manifest_get_file(manifest, 3) == NULL); /* Out of bounds */

    const FileEntry *found = topic_manifest_find_file(manifest, "file_a.txt");
    assert(found != NULL);
    assert(found->file_size == 25);

    assert(topic_manifest_find_file(manifest, "nonexistent.txt") == NULL);

    free_topic_manifest(manifest);
}

/* ----------------------------------------------------------------------------
 * Test 4: Empty Topic Directory Handling
 * ---------------------------------------------------------------------------- */
static void test_empty_topic_manifest(void) {
    TopicManifest *manifest = NULL;
    TopicError err = build_topic_manifest(test_root_dir, "empty_topic", &manifest);
    assert(err == TOPIC_OK);
    assert(manifest != NULL);

    assert(manifest->file_count == 0);
    assert(manifest->total_topic_bytes == 0);
    assert(manifest->manifest_fingerprint == 0);
    assert(manifest->files == NULL);

    free_topic_manifest(manifest);
}

/* ----------------------------------------------------------------------------
 * Test 5: Filtering of Subdirectories, Symlinks, Hidden Files, FIFOs
 * ---------------------------------------------------------------------------- */
static void test_regular_file_filtering(void) {
    TopicManifest *manifest = NULL;
    TopicError err = build_topic_manifest(test_root_dir, "filtered_topic", &manifest);
    assert(err == TOPIC_OK);
    assert(manifest != NULL);

    /*
     * filtered_topic contained:
     * - regular.bin (regular file)
     * - sub_dir/ (subdirectory) -> MUST BE EXCLUDED
     * - .hidden_data (hidden file) -> MUST BE EXCLUDED
     * - symlink_link (symlink) -> MUST BE EXCLUDED
     * - test_fifo (fifo) -> MUST BE EXCLUDED
     * Exactly 1 file must be in the manifest!
     */
    assert(manifest->file_count == 1);
    assert(strcmp(manifest->files[0].filename, "regular.bin") == 0);
    assert(manifest->files[0].file_size == 18);

    free_topic_manifest(manifest);
}

/* ----------------------------------------------------------------------------
 * Test 6: Nonexistent Topic
 * ---------------------------------------------------------------------------- */
static void test_nonexistent_topic(void) {
    TopicManifest *manifest = NULL;
    TopicError err = build_topic_manifest(test_root_dir, "does_not_exist", &manifest);
    assert(err == TOPIC_ERR_NOT_FOUND);
    assert(manifest == NULL);
}

/* ----------------------------------------------------------------------------
 * Test 7: Symlink Escape Attempt Rejection
 * ---------------------------------------------------------------------------- */
static void test_symlink_escape_rejection(void) {
    TopicManifest *manifest = NULL;
    /* symlink_escape_topic is a symlink pointing to /tmp */
    TopicError err = build_topic_manifest(test_root_dir, "symlink_escape_topic", &manifest);
    /* Must be detected and rejected as TOPIC_ERR_ESCAPE */
    assert(err == TOPIC_ERR_ESCAPE);
    assert(manifest == NULL);
}

/* ----------------------------------------------------------------------------
 * Test 8: Traversal Injection Rejected Before Filesystem Access
 * ---------------------------------------------------------------------------- */
static void test_traversal_payload_rejection(void) {
    TopicManifest *manifest = NULL;
    assert(build_topic_manifest(test_root_dir, "../../etc/passwd", &manifest) == TOPIC_ERR_INVALID_NAME);
    assert(manifest == NULL);

    assert(build_topic_manifest(test_root_dir, "/etc/shadow", &manifest) == TOPIC_ERR_INVALID_NAME);
    assert(manifest == NULL);

    assert(build_topic_manifest(test_root_dir, "..", &manifest) == TOPIC_ERR_INVALID_NAME);
    assert(manifest == NULL);
}

/* ----------------------------------------------------------------------------
 * Main Entry Point
 * ---------------------------------------------------------------------------- */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("===============================================================\n");
    printf("IT305 Topic Manager Unit & Security Test Suite\n");
    printf("Owner: Jay Vaghela (202401237) - Phase 1 Task 1.2\n");
    printf("===============================================================\n\n");

    setup_test_fixtures();

    run_test("Topic Name Traversal & Metacharacter Validation", test_validate_topic_name);
    run_test("Topic Path Construction & Buffer Protection", test_build_topic_path);
    run_test("Valid Topic Manifest, Lexicographical Sorting & CRC32", test_valid_topic_manifest);
    run_test("Empty Topic Directory Handling", test_empty_topic_manifest);
    run_test("Regular-File Filtering (Excluding Symlinks, Subdirs, FIFOs)", test_regular_file_filtering);
    run_test("Nonexistent Topic Rejection (TOPIC_ERR_NOT_FOUND)", test_nonexistent_topic);
    run_test("Symlink Directory Escape Rejection (TOPIC_ERR_ESCAPE)", test_symlink_escape_rejection);
    run_test("Traversal Payloads Rejection (../../etc/passwd, /etc/shadow)", test_traversal_payload_rejection);

    teardown_test_fixtures();

    printf("\n===============================================================\n");
    printf("Test Results: %d/%d Passed (100%% Success Rate)\n", passed_tests, total_tests);
    printf("Status: common/topic_manager.* Fully Verified and Gate 1.2 Ready\n");
    printf("===============================================================\n");

    return 0;
}
