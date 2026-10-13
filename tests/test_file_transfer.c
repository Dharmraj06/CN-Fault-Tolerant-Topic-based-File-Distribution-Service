/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: tests/test_file_transfer.c
 * Description: Unit & Robustness Test Suite for common/file_transfer.*
 *
 * Phase: Phase 2 (Task 2.1 Bounded File Streaming Verification)
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "file_transfer.h"
#include "protocol.h"
#include "topic_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#define TEST_RUN(name) printf("[RUNNING] %s ...\n", name)
#define TEST_PASS(name) printf("  [PASS] %s\n", name)

static char g_test_tmp_dir[256];

static void setup_test_environment(void) {
    snprintf(g_test_tmp_dir, sizeof(g_test_tmp_dir), "/tmp/test_ft_suite_%d", (int)getpid());
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s && mkdir -p %s", g_test_tmp_dir, g_test_tmp_dir);
    int rc = system(cmd);
    assert(rc == 0);
}

static void teardown_test_environment(void) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_tmp_dir);
    (void)system(cmd);
}

/* ----------------------------------------------------------------------------
 * Test 1: Recursive Directory Creation
 * ---------------------------------------------------------------------------- */
static void test_create_directory_recursive(void) {
    TEST_RUN("Recursive Directory Creation");

    char nested[512];
    snprintf(nested, sizeof(nested), "%s/level1/level2/level3/topic_dir", g_test_tmp_dir);

    FileTransferError err = create_directory_recursive(nested);
    assert(err == FT_OK);

    struct stat st;
    assert(stat(nested, &st) == 0);
    assert(S_ISDIR(st.st_mode));

    /* Calling again on already existing directory must return FT_OK */
    err = create_directory_recursive(nested);
    assert(err == FT_OK);

    /* Invalid arguments */
    assert(create_directory_recursive(NULL) == FT_ERR_INVALID_ARG);
    assert(create_directory_recursive("") == FT_ERR_INVALID_ARG);

    TEST_PASS("Recursive Directory Creation");
}

/* ----------------------------------------------------------------------------
 * Test 2: Filename & Path Traversal Sanitization
 * ---------------------------------------------------------------------------- */
static void test_path_sanitization(void) {
    TEST_RUN("Filename Validation & Path Sanitization");

    /* Valid filenames */
    assert(validate_filename("image.png") == FT_OK);
    assert(validate_filename("data_123.dat") == FT_OK);
    assert(validate_filename("report.final.pdf") == FT_OK);

    /* Path traversal and illegal characters */
    assert(validate_filename(NULL) == FT_ERR_INVALID_ARG);
    assert(validate_filename("") == FT_ERR_INVALID_ARG);
    assert(validate_filename(".") == FT_ERR_PATH_ESCAPE);
    assert(validate_filename("..") == FT_ERR_PATH_ESCAPE);
    assert(validate_filename("../../etc/passwd") == FT_ERR_PATH_ESCAPE);
    assert(validate_filename("/etc/shadow") == FT_ERR_PATH_ESCAPE);
    assert(validate_filename("folder/file.txt") == FT_ERR_PATH_ESCAPE);
    assert(validate_filename("folder\\file.txt") == FT_ERR_PATH_ESCAPE);

    /* Test build_destination_path */
    char out_path[FT_MAX_PATH_LEN];
    FileTransferError err = build_destination_path(g_test_tmp_dir, "cane", "dog1.jpg", out_path, sizeof(out_path));
    assert(err == FT_OK);

    char expected[FT_MAX_PATH_LEN];
    snprintf(expected, sizeof(expected), "%s/cane/dog1.jpg", g_test_tmp_dir);
    assert(strcmp(out_path, expected) == 0);

    /* Traversal attempts in topic or output_dir */
    assert(build_destination_path(g_test_tmp_dir, "../../etc", "passwd", out_path, sizeof(out_path)) == FT_ERR_PATH_ESCAPE);
    assert(build_destination_path(g_test_tmp_dir, "cane", "../escape.jpg", out_path, sizeof(out_path)) == FT_ERR_PATH_ESCAPE);
    assert(build_destination_path("/tmp/../etc", "cane", "dog.jpg", out_path, sizeof(out_path)) == FT_ERR_PATH_ESCAPE);

    TEST_PASS("Filename Validation & Path Sanitization");
}

/* ----------------------------------------------------------------------------
 * Test 3: Empty File (0 Bytes) Lifecycle & Streaming
 * ---------------------------------------------------------------------------- */
static void test_empty_file(void) {
    TEST_RUN("Empty File (0 Bytes) Operations");

    char file_path[FT_MAX_PATH_LEN];
    snprintf(file_path, sizeof(file_path), "%s/empty.dat", g_test_tmp_dir);

    /* Create empty file */
    int fd = open_file_for_writing(file_path, true);
    assert(fd >= 0);
    assert(close_file(&fd) == FT_OK);
    assert(fd == -1);

    /* Open for reading */
    uint64_t size = 999;
    fd = open_file_for_reading(file_path, &size);
    assert(fd >= 0);
    assert(size == 0);

    /* Read from empty file */
    char buf[128];
    ssize_t n = read_chunk_from_file(fd, buf, sizeof(buf));
    assert(n == 0); /* 0 on clean EOF */
    assert(close_file(&fd) == FT_OK);

    /* Streaming 0-byte file over socketpair */
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    int rfd = open_file_for_reading(file_path, &size);
    assert(rfd >= 0);

    char out_path[FT_MAX_PATH_LEN];
    snprintf(out_path, sizeof(out_path), "%s/empty_recv.dat", g_test_tmp_dir);
    int wfd = open_file_for_writing(out_path, true);
    assert(wfd >= 0);

    assert(send_file_chunks(sv[0], rfd, size, 0) == FT_OK);
    assert(receive_file_chunks(sv[1], wfd, size) == FT_OK);

    close_file(&rfd);
    close_file(&wfd);
    close(sv[0]);
    close(sv[1]);

    uint64_t recv_size = 999;
    int check_fd = open_file_for_reading(out_path, &recv_size);
    assert(check_fd >= 0);
    assert(recv_size == 0);
    close_file(&check_fd);

    TEST_PASS("Empty File (0 Bytes) Operations");
}

/* ----------------------------------------------------------------------------
 * Test 4: Small File Operations (< 64 KiB)
 * ---------------------------------------------------------------------------- */
static void test_small_file(void) {
    TEST_RUN("Small File (< 64 KiB) Read, Write and Verification");

    char file_path[FT_MAX_PATH_LEN];
    snprintf(file_path, sizeof(file_path), "%s/small.dat", g_test_tmp_dir);

    const size_t test_size = 4096;
    char write_data[4096];
    for (size_t i = 0; i < test_size; i++) {
        write_data[i] = (char)((i * 37 + 11) & 0xFF);
    }

    int wfd = open_file_for_writing(file_path, true);
    assert(wfd >= 0);
    FileTransferError err = write_chunk_to_file(wfd, write_data, test_size);
    assert(err == FT_OK);
    assert(close_file(&wfd) == FT_OK);

    /* Read back and verify */
    uint64_t file_size = 0;
    int rfd = open_file_for_reading(file_path, &file_size);
    assert(rfd >= 0);
    assert(file_size == test_size);

    char read_data[4096];
    ssize_t n = read_chunk_from_file(rfd, read_data, sizeof(read_data));
    assert(n == (ssize_t)test_size);
    assert(memcmp(write_data, read_data, test_size) == 0);

    /* Next read must return 0 (EOF) */
    n = read_chunk_from_file(rfd, read_data, sizeof(read_data));
    assert(n == 0);
    assert(close_file(&rfd) == FT_OK);

    TEST_PASS("Small File (< 64 KiB) Read, Write and Verification");
}

/* ----------------------------------------------------------------------------
 * Thread Helper for Concurrent Socket Streaming Tests
 * ---------------------------------------------------------------------------- */
typedef struct {
    int sock_fd;
    int file_fd;
    uint64_t file_size;
    uint64_t start_offset;
    FileTransferError result;
} StreamThreadArg;

static void* sender_thread_worker(void *arg) {
    StreamThreadArg *a = (StreamThreadArg *)arg;
    a->result = send_file_chunks(a->sock_fd, a->file_fd, a->file_size, a->start_offset);
    return NULL;
}

/* ----------------------------------------------------------------------------
 * Test 5: Exact Chunk Boundary (64 KiB = 65,536 Bytes)
 * ---------------------------------------------------------------------------- */
static void test_exact_chunk_boundary(void) {
    TEST_RUN("Exact Chunk Boundary (64 KiB = 65,536 Bytes)");

    char src_path[FT_MAX_PATH_LEN];
    snprintf(src_path, sizeof(src_path), "%s/exact_64k.dat", g_test_tmp_dir);

    const size_t file_len = FT_CHUNK_SIZE;
    char *orig_buf = malloc(file_len);
    assert(orig_buf != NULL);
    for (size_t i = 0; i < file_len; i++) {
        orig_buf[i] = (char)((i * 101) & 0xFF);
    }

    int wfd = open_file_for_writing(src_path, true);
    assert(wfd >= 0);
    assert(write_chunk_to_file(wfd, orig_buf, file_len) == FT_OK);
    close_file(&wfd);

    /* Stream over loopback socketpair */
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    char dst_path[FT_MAX_PATH_LEN];
    snprintf(dst_path, sizeof(dst_path), "%s/exact_64k_recv.dat", g_test_tmp_dir);
    int dst_fd = open_file_for_writing(dst_path, true);
    assert(dst_fd >= 0);

    int src_fd = open_file_for_reading(src_path, NULL);
    assert(src_fd >= 0);

    StreamThreadArg sarg = {
        .sock_fd = sv[0],
        .file_fd = src_fd,
        .file_size = file_len,
        .start_offset = 0,
        .result = FT_OK
    };
    pthread_t stid;
    assert(pthread_create(&stid, NULL, sender_thread_worker, &sarg) == 0);

    FileTransferError rerr = receive_file_chunks(sv[1], dst_fd, file_len);
    assert(rerr == FT_OK);

    assert(pthread_join(stid, NULL) == 0);
    assert(sarg.result == FT_OK);

    close_file(&src_fd);
    close_file(&dst_fd);
    close(sv[0]);
    close(sv[1]);

    /* Compare files bit-for-bit */
    char *check_buf = malloc(file_len);
    assert(check_buf != NULL);
    int check_fd = open_file_for_reading(dst_path, NULL);
    assert(check_fd >= 0);
    assert(read_chunk_from_file(check_fd, check_buf, file_len) == (ssize_t)file_len);
    assert(memcmp(orig_buf, check_buf, file_len) == 0);
    close_file(&check_fd);

    free(orig_buf);
    free(check_buf);

    TEST_PASS("Exact Chunk Boundary (64 KiB = 65,536 Bytes)");
}

/* ----------------------------------------------------------------------------
 * Test 6: Multiple Chunks with Final Partial Chunk (150 KiB)
 * ---------------------------------------------------------------------------- */
static void test_multi_chunk_partial(void) {
    TEST_RUN("Multiple Chunks with Final Partial Chunk (150 KiB)");

    char src_path[FT_MAX_PATH_LEN];
    snprintf(src_path, sizeof(src_path), "%s/multi_150k.dat", g_test_tmp_dir);

    const size_t total_size = 153600; /* 2 full 64 KiB chunks + 22 KiB final chunk */
    char *src_data = malloc(total_size);
    assert(src_data != NULL);
    for (size_t i = 0; i < total_size; i++) {
        src_data[i] = (char)((i * 13 + 7) & 0xFF);
    }

    int wfd = open_file_for_writing(src_path, true);
    assert(wfd >= 0);
    assert(write_chunk_to_file(wfd, src_data, total_size) == FT_OK);
    close_file(&wfd);

    /* Stream over loopback socketpair */
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    char dst_path[FT_MAX_PATH_LEN];
    snprintf(dst_path, sizeof(dst_path), "%s/multi_150k_recv.dat", g_test_tmp_dir);
    int dst_fd = open_file_for_writing(dst_path, true);
    assert(dst_fd >= 0);

    int src_fd = open_file_for_reading(src_path, NULL);
    assert(src_fd >= 0);

    StreamThreadArg sarg = {
        .sock_fd = sv[0],
        .file_fd = src_fd,
        .file_size = total_size,
        .start_offset = 0,
        .result = FT_OK
    };
    pthread_t stid;
    assert(pthread_create(&stid, NULL, sender_thread_worker, &sarg) == 0);

    FileTransferError rerr = receive_file_chunks(sv[1], dst_fd, total_size);
    assert(rerr == FT_OK);

    assert(pthread_join(stid, NULL) == 0);
    assert(sarg.result == FT_OK);

    close_file(&src_fd);
    close_file(&dst_fd);
    close(sv[0]);
    close(sv[1]);

    /* Compare received file */
    char *dst_data = malloc(total_size);
    assert(dst_data != NULL);
    int check_fd = open_file_for_reading(dst_path, NULL);
    assert(check_fd >= 0);

    size_t total_read = 0;
    while (total_read < total_size) {
        ssize_t n = read_chunk_from_file(check_fd, dst_data + total_read, total_size - total_read);
        assert(n > 0);
        total_read += (size_t)n;
    }
    assert(total_read == total_size);
    assert(memcmp(src_data, dst_data, total_size) == 0);
    close_file(&check_fd);

    free(src_data);
    free(dst_data);

    TEST_PASS("Multiple Chunks with Final Partial Chunk (150 KiB)");
}

/* ----------------------------------------------------------------------------
 * Test 7: Large File Stream (512 KiB = 8 Full Chunks)
 * ---------------------------------------------------------------------------- */
static void test_large_file_stream(void) {
    TEST_RUN("Large File Stream (512 KiB = 8 Full 64 KiB Chunks)");

    char src_path[FT_MAX_PATH_LEN];
    snprintf(src_path, sizeof(src_path), "%s/large_512k.dat", g_test_tmp_dir);

    const size_t total_size = 512 * 1024;
    char *src_data = malloc(total_size);
    assert(src_data != NULL);
    for (size_t i = 0; i < total_size; i++) {
        src_data[i] = (char)((i ^ (i >> 8)) & 0xFF);
    }

    int wfd = open_file_for_writing(src_path, true);
    assert(wfd >= 0);
    assert(write_chunk_to_file(wfd, src_data, total_size) == FT_OK);
    close_file(&wfd);

    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    char dst_path[FT_MAX_PATH_LEN];
    snprintf(dst_path, sizeof(dst_path), "%s/large_512k_recv.dat", g_test_tmp_dir);
    int dst_fd = open_file_for_writing(dst_path, true);
    assert(dst_fd >= 0);

    int src_fd = open_file_for_reading(src_path, NULL);
    assert(src_fd >= 0);

    StreamThreadArg sarg = {
        .sock_fd = sv[0],
        .file_fd = src_fd,
        .file_size = total_size,
        .start_offset = 0,
        .result = FT_OK
    };
    pthread_t stid;
    assert(pthread_create(&stid, NULL, sender_thread_worker, &sarg) == 0);

    FileTransferError rerr = receive_file_chunks(sv[1], dst_fd, total_size);
    assert(rerr == FT_OK);

    assert(pthread_join(stid, NULL) == 0);
    assert(sarg.result == FT_OK);

    close_file(&src_fd);
    close_file(&dst_fd);
    close(sv[0]);
    close(sv[1]);

    char *dst_data = malloc(total_size);
    assert(dst_data != NULL);
    int check_fd = open_file_for_reading(dst_path, NULL);
    assert(check_fd >= 0);

    size_t total_read = 0;
    while (total_read < total_size) {
        ssize_t n = read_chunk_from_file(check_fd, dst_data + total_read, total_size - total_read);
        assert(n > 0);
        total_read += (size_t)n;
    }
    assert(total_read == total_size);
    assert(memcmp(src_data, dst_data, total_size) == 0);
    close_file(&check_fd);

    free(src_data);
    free(dst_data);

    TEST_PASS("Large File Stream (512 KiB = 8 Full 64 KiB Chunks)");
}

/* ----------------------------------------------------------------------------
 * Test 8: Sync, Truncate, and Seek Operations
 * ---------------------------------------------------------------------------- */
static void test_sync_truncate_seek(void) {
    TEST_RUN("fsync, ftruncate, and lseek Operations");

    char file_path[FT_MAX_PATH_LEN];
    snprintf(file_path, sizeof(file_path), "%s/trunc_test.dat", g_test_tmp_dir);

    const size_t initial_size = 100000;
    char *buf = malloc(initial_size);
    assert(buf != NULL);
    memset(buf, 'A', initial_size);

    int fd = open_file_for_writing(file_path, true);
    assert(fd >= 0);
    assert(write_chunk_to_file(fd, buf, initial_size) == FT_OK);

    /* Test sync */
    assert(sync_file_to_disk(fd) == FT_OK);

    /* Truncate and seek to 45,000 bytes */
    const uint64_t new_size = 45000;
    assert(truncate_and_seek(fd, new_size) == FT_OK);

    /* Write 10 bytes at new offset */
    assert(write_chunk_to_file(fd, "0123456789", 10) == FT_OK);
    assert(close_file(&fd) == FT_OK);

    /* Verify resulting file size is 45,010 bytes */
    uint64_t final_size = 0;
    int check_fd = open_file_for_reading(file_path, &final_size);
    assert(check_fd >= 0);
    assert(final_size == 45010);
    close_file(&check_fd);

    free(buf);
    TEST_PASS("fsync, ftruncate, and lseek Operations");
}

/* ----------------------------------------------------------------------------
 * Test 9: Robustness, Error Handling & Cleanup
 * ---------------------------------------------------------------------------- */
static void test_error_handling_cleanup(void) {
    TEST_RUN("Invalid Arguments, Error Handling & Cleanup");

    /* Operations on negative descriptor */
    char dummy[64];
    assert(read_chunk_from_file(-1, dummy, sizeof(dummy)) == (ssize_t)FT_ERR_INVALID_ARG);
    assert(write_chunk_to_file(-1, dummy, sizeof(dummy)) == FT_ERR_INVALID_ARG);
    assert(sync_file_to_disk(-1) == FT_ERR_INVALID_ARG);
    assert(truncate_and_seek(-1, 100) == FT_ERR_INVALID_ARG);
    assert(close_file(NULL) == FT_ERR_INVALID_ARG);

    int invalid_fd = -1;
    assert(close_file(&invalid_fd) == FT_ERR_INVALID_ARG);

    /* Open nonexistent file for reading */
    assert(open_file_for_reading("/tmp/nonexistent_file_xyz123.dat", NULL) == FT_ERR_FILE_OPEN);

    /* Open directory for reading */
    assert(open_file_for_reading(g_test_tmp_dir, NULL) == FT_ERR_FILE_OPEN);

    /* Error string conversions */
    assert(file_transfer_error_to_string(FT_OK) != NULL);
    assert(file_transfer_error_to_string(FT_ERR_WRITE) != NULL);
    assert(file_transfer_error_to_string(FT_ERR_PATH_ESCAPE) != NULL);

    TEST_PASS("Invalid Arguments, Error Handling & Cleanup");
}

/* ----------------------------------------------------------------------------
 * Main Entry Point
 * ---------------------------------------------------------------------------- */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("===============================================================\n");
    printf("IT305 File Transfer Engine Test Suite (common/file_transfer.*)\n");
    printf("===============================================================\n\n");

    setup_test_environment();

    test_create_directory_recursive();
    test_path_sanitization();
    test_empty_file();
    test_small_file();
    test_exact_chunk_boundary();
    test_multi_chunk_partial();
    test_large_file_stream();
    test_sync_truncate_seek();
    test_error_handling_cleanup();

    teardown_test_environment();

    printf("\n===============================================================\n");
    printf("All 9 File Transfer Tests Passed Successfully (100%% Success Rate)\n");
    printf("Status: common/file_transfer.* Verified and Task 2.1 Complete\n");
    printf("===============================================================\n");

    return 0;
}
