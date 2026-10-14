/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: tests/test_session.c
 * Description: Unit Tests for Simplified In-Memory Session Module.
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "session.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#define TEST_PASS(name) printf("  [PASS] %s\n", name)

static void test_create_and_lookup(void) {
    session_table_init();

    uint64_t id = 0;
    assert(session_create("sample_topic", 0x12345678, 3, &id) == 0);
    assert(id != 0);

    SessionState snap;
    assert(session_get_snapshot(id, &snap) == 0);
    assert(snap.session_id == id);
    assert(strcmp(snap.topic, "sample_topic") == 0);
    assert(snap.manifest_fingerprint == 0x12345678);
    assert(snap.file_count == 3);
    assert(snap.current_file_index == 0);
    assert(snap.current_file_offset == 0);
    assert(snap.status == SESSION_ACTIVE);

    /* Lookup nonexistent session */
    assert(session_get_snapshot(0x99999999, &snap) == -1);

    TEST_PASS("Session Creation and Snapshot Lookup");
}

static void test_checkpoint_commit(void) {
    session_table_init();

    uint64_t id = 0;
    assert(session_create("topic1", 0xABCD, 2, &id) == 0);

    /* Forward progress */
    assert(session_commit_chunk(id, 0, 65536, 200000) == 0);
    SessionState snap;
    session_get_snapshot(id, &snap);
    assert(snap.current_file_offset == 65536);

    /* Duplicate ACK (same offset) returns 1 without corrupting */
    assert(session_commit_chunk(id, 0, 65536, 200000) == 1);
    session_get_snapshot(id, &snap);
    assert(snap.current_file_offset == 65536);

    /* Regression offset rejected */
    assert(session_commit_chunk(id, 0, 32768, 200000) == -1);

    /* Wrong file index rejected */
    assert(session_commit_chunk(id, 1, 131072, 200000) == -1);

    /* Exceeding file size rejected */
    assert(session_commit_chunk(id, 0, 250000, 200000) == -1);

    /* Another valid forward progress */
    assert(session_commit_chunk(id, 0, 200000, 200000) == 0);
    session_get_snapshot(id, &snap);
    assert(snap.current_file_offset == 200000);

    TEST_PASS("Monotonic Checkpoint Progression & Rejections");
}

static void test_file_completion(void) {
    session_table_init();

    uint64_t id = 0;
    assert(session_create("topic2", 0x1111, 2, &id) == 0);

    assert(session_commit_chunk(id, 0, 1000, 1000) == 0);

    /* Advance to file 1 */
    assert(session_complete_file(id, 0) == 0);

    SessionState snap;
    session_get_snapshot(id, &snap);
    assert(snap.current_file_index == 1);
    assert(snap.current_file_offset == 0);

    /* Complete file 1 */
    assert(session_commit_chunk(id, 1, 2000, 2000) == 0);
    assert(session_complete_file(id, 1) == 0);
    assert(session_mark_complete(id) == 0);

    session_get_snapshot(id, &snap);
    assert(snap.status == SESSION_COMPLETE);

    TEST_PASS("File Boundary Transitions and Completion");
}

static void test_resume_validation(void) {
    session_table_init();

    uint64_t id = 0;
    assert(session_create("topic3", 0x9999, 3, &id) == 0);
    assert(session_commit_chunk(id, 0, 65536, 100000) == 0);

    uint32_t auth_file = 0;
    uint64_t auth_offset = 0;
    uint16_t flags = 0;

    /* 1. Exact match resume */
    assert(session_validate_resume(id, "topic3", 0x9999, 0, 65536, &auth_file, &auth_offset, &flags) == 0);
    assert(auth_file == 0 && auth_offset == 65536 && flags == FLAG_NONE);

    /* 2. Client behind server (e.g. proposed 0, server has 65536): authoritative returned */
    assert(session_validate_resume(id, "topic3", 0x9999, 0, 0, &auth_file, &auth_offset, &flags) == 0);
    assert(auth_file == 0 && auth_offset == 65536 && flags == FLAG_NONE);

    /* 3. Client ahead of server: authoritative returned and FLAG_CONFLICT set */
    assert(session_validate_resume(id, "topic3", 0x9999, 0, 80000, &auth_file, &auth_offset, &flags) == 0);
    assert(auth_file == 0 && auth_offset == 65536 && (flags & FLAG_CONFLICT));

    /* 4. Topic mismatch rejected */
    assert(session_validate_resume(id, "wrong_topic", 0x9999, 0, 65536, &auth_file, &auth_offset, &flags) == -5);

    /* 5. Fingerprint mismatch rejected */
    assert(session_validate_resume(id, "topic3", 0xAAAA, 0, 65536, &auth_file, &auth_offset, &flags) == -6);

    /* 6. Non-existent session rejected */
    assert(session_validate_resume(0x1234567, "topic3", 0x9999, 0, 65536, &auth_file, &auth_offset, &flags) == -2);

    /* 7. Completed session rejected */
    session_mark_complete(id);
    assert(session_validate_resume(id, "topic3", 0x9999, 0, 65536, &auth_file, &auth_offset, &flags) == -4);

    TEST_PASS("Authoritative Resume Validation & Conflict Flag");
}

/* Multithreaded stress test */
#define NUM_WORKERS 8
#define COMMITS_PER_WORKER 500

static uint64_t g_shared_id = 0;

static void* worker_func(void *arg) {
    (void)arg;
    for (int i = 0; i < COMMITS_PER_WORKER; i++) {
        SessionState snap;
        session_get_snapshot(g_shared_id, &snap);
    }
    return NULL;
}

static void test_concurrency(void) {
    session_table_init();
    assert(session_create("concurrent_topic", 0x5555, 1, &g_shared_id) == 0);

    pthread_t th[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&th[i], NULL, worker_func, NULL);
    }
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(th[i], NULL);
    }

    TEST_PASS("Multi-Threaded Safe Concurrent Snapshots");
}

int main(void) {
    printf("===============================================================\n");
    printf("IT305 Session Unit Tests (Simplified Phase 4 Implementation)\n");
    printf("===============================================================\n");

    test_create_and_lookup();
    test_checkpoint_commit();
    test_file_completion();
    test_resume_validation();
    test_concurrency();

    printf("\nAll Session Unit Tests Passed Successfully!\n");
    return 0;
}
