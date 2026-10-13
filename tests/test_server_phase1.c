/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: tests/test_server_phase1.c
 * Description: Loopback Socket Integration Test Suite for Part1/server.c.
 *
 * Author: Jay Vaghela (Student ID: 202401237)
 * Phase: Phase 1 (Task 1.3 Server Verification)
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "protocol.h"
#include "topic_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define TEST_PASS(name) printf("  [PASS] %s\n", name)
#define TEST_FAIL(name, msg) do { \
    fprintf(stderr, "  [FAIL] %s: %s (errno=%d: %s)\n", name, msg, errno, strerror(errno)); \
    exit(1); \
} while (0)

static int total_tests = 0;
static int passed_tests = 0;

static int server_port = 19999;

static int connect_to_server(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)server_port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* ----------------------------------------------------------------------------
 * Test 1: Valid Topic Loopback Transfer
 * ---------------------------------------------------------------------------- */
static void test_valid_topic_transfer(void) {
    int fd = connect_to_server();
    if (fd < 0) TEST_FAIL("test_valid_topic_transfer", "Cannot connect to server");

    /* Send MSG_GET for "valid_topic" */
    MsgGetPayload req;
    memset(&req, 0, sizeof(req));
    strncpy(req.topic, "valid_topic", sizeof(req.topic) - 1);
    assert(send_frame(fd, MSG_GET, FLAG_NONE, &req, sizeof(req)) == 0);

    /* Receive MSG_TOPIC_OK */
    FrameHeader hdr;
    assert(recv_frame_header(fd, &hdr) == 0);
    assert(hdr.type == MSG_TOPIC_OK);

    MsgTopicOkPayload ok_net, ok_decoded;
    assert(recv_frame_payload(fd, &hdr, &ok_net, sizeof(ok_net)) == 0);
    proto_ntoh_topic_ok(&ok_decoded, &ok_net);

    assert(ok_decoded.file_count == 3);
    assert(ok_decoded.manifest_fingerprint != 0);
    assert(ok_decoded.total_topic_bytes == 135);

    /* Consume 3 file streams */
    for (uint32_t i = 0; i < ok_decoded.file_count; i++) {
        /* MSG_FILE_BEGIN */
        assert(recv_frame_header(fd, &hdr) == 0);
        assert(hdr.type == MSG_FILE_BEGIN);
        MsgFileBeginPayload fb_net, fb_decoded;
        assert(recv_frame_payload(fd, &hdr, &fb_net, sizeof(fb_net)) == 0);
        proto_ntoh_file_begin(&fb_decoded, &fb_net);
        assert(fb_decoded.file_index == i);

        /* Consume chunks until MSG_FILE_END */
        uint64_t bytes_read = 0;
        while (bytes_read < fb_decoded.file_size) {
            assert(recv_frame_header(fd, &hdr) == 0);
            assert(hdr.type == MSG_FILE_DATA);
            char chunk[DEFAULT_CHUNK_SIZE];
            assert(recv_frame_payload(fd, &hdr, chunk, sizeof(chunk)) == 0);
            bytes_read += hdr.payload_len;
        }

        /* MSG_FILE_END */
        assert(recv_frame_header(fd, &hdr) == 0);
        assert(hdr.type == MSG_FILE_END);
        MsgFileEndPayload fe_net, fe_decoded;
        assert(recv_frame_payload(fd, &hdr, &fe_net, sizeof(fe_net)) == 0);
        proto_ntoh_file_end(&fe_decoded, &fe_net);
        assert(fe_decoded.file_index == i);
        assert(fe_decoded.final_offset == fb_decoded.file_size);
    }

    /* MSG_TRANSFER_END */
    assert(recv_frame_header(fd, &hdr) == 0);
    assert(hdr.type == MSG_TRANSFER_END);
    assert(hdr.payload_len == 0);

    /* Orderly EOF */
    assert(recv_frame_header(fd, &hdr) == 1);

    close(fd);
}

/* ----------------------------------------------------------------------------
 * Test 2: Invalid / Nonexistent Topic
 * ---------------------------------------------------------------------------- */
static void test_invalid_topic(void) {
    int fd = connect_to_server();
    if (fd < 0) TEST_FAIL("test_invalid_topic", "Cannot connect to server");

    MsgGetPayload req;
    memset(&req, 0, sizeof(req));
    strncpy(req.topic, "nonexistent_topic_12345", sizeof(req.topic) - 1);
    assert(send_frame(fd, MSG_GET, FLAG_NONE, &req, sizeof(req)) == 0);

    FrameHeader hdr;
    assert(recv_frame_header(fd, &hdr) == 0);
    assert(hdr.type == MSG_TOPIC_ERROR);

    MsgTopicErrorPayload err_net, err_decoded;
    assert(recv_frame_payload(fd, &hdr, &err_net, sizeof(err_net)) == 0);
    proto_ntoh_topic_error(&err_decoded, &err_net);

    assert(err_decoded.error_code == ERR_TOPIC_NOT_FOUND);

    /* Connection closed by server */
    assert(recv_frame_header(fd, &hdr) == 1);

    close(fd);
}

/* ----------------------------------------------------------------------------
 * Test 3: Path Traversal Security Rejection
 * ---------------------------------------------------------------------------- */
static void test_path_traversal_injection(void) {
    int fd = connect_to_server();
    if (fd < 0) TEST_FAIL("test_path_traversal_injection", "Cannot connect to server");

    MsgGetPayload req;
    memset(&req, 0, sizeof(req));
    strncpy(req.topic, "../../etc/passwd", sizeof(req.topic) - 1);
    assert(send_frame(fd, MSG_GET, FLAG_NONE, &req, sizeof(req)) == 0);

    FrameHeader hdr;
    assert(recv_frame_header(fd, &hdr) == 0);
    assert(hdr.type == MSG_TOPIC_ERROR);

    MsgTopicErrorPayload err_net, err_decoded;
    assert(recv_frame_payload(fd, &hdr, &err_net, sizeof(err_net)) == 0);
    proto_ntoh_topic_error(&err_decoded, &err_net);

    assert(err_decoded.error_code == ERR_FORBIDDEN);

    /* Connection closed by server */
    assert(recv_frame_header(fd, &hdr) == 1);

    close(fd);
}

/* ----------------------------------------------------------------------------
 * Test 4: Abrupt Client Disconnect Handling
 * ---------------------------------------------------------------------------- */
static void test_client_disconnect(void) {
    int fd = connect_to_server();
    if (fd < 0) TEST_FAIL("test_client_disconnect", "Cannot connect to server");

    MsgGetPayload req;
    memset(&req, 0, sizeof(req));
    strncpy(req.topic, "valid_topic", sizeof(req.topic) - 1);
    assert(send_frame(fd, MSG_GET, FLAG_NONE, &req, sizeof(req)) == 0);

    FrameHeader hdr;
    assert(recv_frame_header(fd, &hdr) == 0);
    assert(hdr.type == MSG_TOPIC_OK);

    /* Abruptly close socket while server is preparing to send file data */
    close(fd);

    /* Sleep briefly to let server encounter EPIPE and handle it cleanly */
    usleep(50000);

    /* Now verify server is still responsive and accepting connections */
    int verify_fd = connect_to_server();
    assert(verify_fd >= 0);
    close(verify_fd);
}

/* ----------------------------------------------------------------------------
 * Test 5: Multiple Simultaneous Clients Concurrency
 * ---------------------------------------------------------------------------- */
#define NUM_CONCURRENT_CLIENTS 8

static void* concurrent_client_worker(void *arg) {
    (void)arg;
    int fd = connect_to_server();
    assert(fd >= 0);

    MsgGetPayload req;
    memset(&req, 0, sizeof(req));
    strncpy(req.topic, "valid_topic", sizeof(req.topic) - 1);
    assert(send_frame(fd, MSG_GET, FLAG_NONE, &req, sizeof(req)) == 0);

    FrameHeader hdr;
    assert(recv_frame_header(fd, &hdr) == 0);
    assert(hdr.type == MSG_TOPIC_OK);

    MsgTopicOkPayload ok_net, ok_decoded;
    assert(recv_frame_payload(fd, &hdr, &ok_net, sizeof(ok_net)) == 0);
    proto_ntoh_topic_ok(&ok_decoded, &ok_net);
    assert(ok_decoded.file_count == 3);

    for (uint32_t i = 0; i < ok_decoded.file_count; i++) {
        assert(recv_frame_header(fd, &hdr) == 0); /* BEGIN */
        assert(hdr.type == MSG_FILE_BEGIN);
        MsgFileBeginPayload fb;
        assert(recv_frame_payload(fd, &hdr, &fb, sizeof(fb)) == 0);

        uint64_t bytes = 0;
        uint64_t expected_size = proto_ntoh64(fb.file_size);
        while (bytes < expected_size) {
            assert(recv_frame_header(fd, &hdr) == 0); /* DATA */
            assert(hdr.type == MSG_FILE_DATA);
            char cbuf[DEFAULT_CHUNK_SIZE];
            assert(recv_frame_payload(fd, &hdr, cbuf, sizeof(cbuf)) == 0);
            bytes += hdr.payload_len;
        }

        assert(recv_frame_header(fd, &hdr) == 0); /* END */
        assert(hdr.type == MSG_FILE_END);
        MsgFileEndPayload fe;
        assert(recv_frame_payload(fd, &hdr, &fe, sizeof(fe)) == 0);
    }

    assert(recv_frame_header(fd, &hdr) == 0); /* TRANSFER_END */
    assert(hdr.type == MSG_TRANSFER_END);

    close(fd);
    return NULL;
}

static void test_concurrent_clients(void) {
    pthread_t threads[NUM_CONCURRENT_CLIENTS];
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        assert(pthread_create(&threads[i], NULL, concurrent_client_worker, NULL) == 0);
    }

    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
}

/* ----------------------------------------------------------------------------
 * Main Entry Point
 * ---------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    if (argc >= 2) {
        server_port = atoi(argv[1]);
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("===============================================================\n");
    printf("IT305 Part 1 Server Integration Test Suite\n");
    printf("Owner: Jay Vaghela (202401237) - Phase 1 Task 1.3\n");
    printf("Testing Server on port: %d\n", server_port);
    printf("===============================================================\n\n");

    total_tests++;
    printf("[RUNNING] Test %d: Valid Topic Loopback Transfer\n", total_tests);
    test_valid_topic_transfer();
    passed_tests++;
    TEST_PASS("Valid Topic Loopback Transfer");

    total_tests++;
    printf("[RUNNING] Test %d: Invalid / Nonexistent Topic Rejection (404)\n", total_tests);
    test_invalid_topic();
    passed_tests++;
    TEST_PASS("Invalid / Nonexistent Topic Rejection (404)");

    total_tests++;
    printf("[RUNNING] Test %d: Path Traversal Security Rejection (403)\n", total_tests);
    test_path_traversal_injection();
    passed_tests++;
    TEST_PASS("Path Traversal Security Rejection (403)");

    total_tests++;
    printf("[RUNNING] Test %d: Abrupt Client Disconnection Clean Recovery\n", total_tests);
    test_client_disconnect();
    passed_tests++;
    TEST_PASS("Abrupt Client Disconnection Clean Recovery");

    total_tests++;
    printf("[RUNNING] Test %d: Concurrency Test (%d Simultaneous Clients)\n", total_tests, NUM_CONCURRENT_CLIENTS);
    test_concurrent_clients();
    passed_tests++;
    TEST_PASS("Concurrency Test (Simultaneous Clients)");

    printf("\n===============================================================\n");
    printf("Test Results: %d/%d Passed (100%% Success Rate)\n", passed_tests, total_tests);
    printf("Status: Part1/server.c Fully Verified and Gate 1.3 Ready\n");
    printf("===============================================================\n");

    return 0;
}
