/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: tests/test_protocol.c
 * Description: Comprehensive Unit Test Suite for common/protocol.*
 *
 * Phase: Phase 1 (Task 1.1 Protocol Verification)
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

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

/* ----------------------------------------------------------------------------
 * Test 1: Static Wire Layout & Alignment
 * ---------------------------------------------------------------------------- */
static void test_wire_layout(void) {
    /* Enforce exactly 16 bytes packed size */
    assert(sizeof(FrameHeader) == 16);
    assert(sizeof(MsgGetPayload) == 128);
    assert(sizeof(MsgTopicOkPayload) == 16);
    assert(sizeof(MsgTopicErrorPayload) == 260);
    assert(sizeof(MsgFileBeginPayload) == 276);
    assert(sizeof(MsgFileEndPayload) == 12);
}

/* ----------------------------------------------------------------------------
 * Test 2: 64-Bit and Struct Endianness Conversions
 * ---------------------------------------------------------------------------- */
static void test_endianness(void) {
    uint64_t val = 0x0102030405060708ULL;
    uint64_t net = proto_hton64(val);
    uint64_t host = proto_ntoh64(net);
    assert(host == val);

    /* Verify MsgTopicOkPayload conversions */
    MsgTopicOkPayload ok_src = {
        .file_count = 42,
        .manifest_fingerprint = 0xA1B2C3D4,
        .total_topic_bytes = 1048576ULL * 50
    };
    MsgTopicOkPayload ok_net, ok_decoded;
    proto_hton_topic_ok(&ok_net, &ok_src);
    proto_ntoh_topic_ok(&ok_decoded, &ok_net);

    assert(ok_decoded.file_count == ok_src.file_count);
    assert(ok_decoded.manifest_fingerprint == ok_src.manifest_fingerprint);
    assert(ok_decoded.total_topic_bytes == ok_src.total_topic_bytes);

    /* Verify MsgFileBeginPayload conversions */
    MsgFileBeginPayload fb_src;
    fb_src.file_index = 7;
    fb_src.file_size = 987654321ULL;
    fb_src.start_offset = 65536ULL;
    strncpy(fb_src.filename, "cat_image_01.jpg", sizeof(fb_src.filename) - 1);
    fb_src.filename[sizeof(fb_src.filename) - 1] = '\0';

    MsgFileBeginPayload fb_net, fb_decoded;
    proto_hton_file_begin(&fb_net, &fb_src);
    proto_ntoh_file_begin(&fb_decoded, &fb_net);

    assert(fb_decoded.file_index == fb_src.file_index);
    assert(fb_decoded.file_size == fb_src.file_size);
    assert(fb_decoded.start_offset == fb_src.start_offset);
    assert(strcmp(fb_decoded.filename, fb_src.filename) == 0);
}

/* ----------------------------------------------------------------------------
 * Test 3: Message Type Validation
 * ---------------------------------------------------------------------------- */
static void test_message_types(void) {
    assert(is_valid_message_type(MSG_GET) == true);
    assert(is_valid_message_type(MSG_TOPIC_OK) == true);
    assert(is_valid_message_type(MSG_TOPIC_ERROR) == true);
    assert(is_valid_message_type(MSG_FILE_BEGIN) == true);
    assert(is_valid_message_type(MSG_FILE_DATA) == true);
    assert(is_valid_message_type(MSG_FILE_END) == true);
    assert(is_valid_message_type(MSG_TRANSFER_END) == true);
    assert(is_valid_message_type(MSG_CLOSE) == true);

    /* Invalid type numbers */
    assert(is_valid_message_type(0x00) == false);
    assert(is_valid_message_type(0x20) == false);
    assert(is_valid_message_type(0xFF) == false);
}

/* ----------------------------------------------------------------------------
 * Test 4: Standard Frame Round-Trip Over Socket Pair
 * ---------------------------------------------------------------------------- */
static void test_standard_frame_roundtrip(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        TEST_FAIL("test_standard_frame_roundtrip", "socketpair failed");
    }

    MsgGetPayload req;
    memset(&req, 0, sizeof(req));
    strncpy(req.topic, "cane", sizeof(req.topic) - 1);

    int rc = send_frame(sv[0], MSG_GET, FLAG_NONE, &req, sizeof(req));
    assert(rc == 0);

    FrameHeader hdr;
    rc = recv_frame_header(sv[1], &hdr);
    assert(rc == 0);
    assert(hdr.magic == PROTOCOL_MAGIC);
    assert(hdr.version == PROTOCOL_VERSION);
    assert(hdr.type == MSG_GET);
    assert(hdr.flags == FLAG_NONE);
    assert(hdr.reserved == 0x0000);
    assert(hdr.payload_len == sizeof(req));

    MsgGetPayload received_req;
    rc = recv_frame_payload(sv[1], &hdr, &received_req, sizeof(received_req));
    assert(rc == 0);
    assert(strcmp(received_req.topic, "cane") == 0);

    close(sv[0]);
    close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Test 5: Empty Payload Frame (e.g. MSG_TRANSFER_END)
 * ---------------------------------------------------------------------------- */
static void test_empty_payload_frame(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        TEST_FAIL("test_empty_payload_frame", "socketpair failed");
    }

    /* Send frame with zero payload length and NULL buffer */
    int rc = send_frame(sv[0], MSG_TRANSFER_END, FLAG_NONE, NULL, 0);
    assert(rc == 0);

    FrameHeader hdr;
    rc = recv_frame_header(sv[1], &hdr);
    assert(rc == 0);
    assert(hdr.type == MSG_TRANSFER_END);
    assert(hdr.payload_len == 0);

    /* recv_frame_payload with payload_len == 0 should return 0 immediately */
    char dummy_buf[10];
    rc = recv_frame_payload(sv[1], &hdr, dummy_buf, sizeof(dummy_buf));
    assert(rc == 0);

    /* recv_frame_payload_alloc with payload_len == 0 should return 0 and set out_buf = NULL */
    void *out_mem = (void *)0x1;
    rc = recv_frame_payload_alloc(sv[1], &hdr, &out_mem);
    assert(rc == 0);
    assert(out_mem == NULL);

    close(sv[0]);
    close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Test 6: Large Frame (1 MB) Concurrent Streaming & Dynamic Allocation
 * ---------------------------------------------------------------------------- */
typedef struct {
    int fd;
    const void *buf;
    size_t len;
} LargeSenderArg;

static void *large_sender_thread(void *arg) {
    LargeSenderArg *s = (LargeSenderArg *)arg;
    int rc = send_frame(s->fd, MSG_FILE_DATA, FLAG_NONE, s->buf, s->len);
    assert(rc == 0);
    return NULL;
}

static void test_large_frame(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        TEST_FAIL("test_large_frame", "socketpair failed");
    }

    size_t payload_size = 1024 * 1024; /* 1 MB */
    char *orig_buf = malloc(payload_size);
    assert(orig_buf != NULL);

    for (size_t i = 0; i < payload_size; i++) {
        orig_buf[i] = (char)(i & 0xFF);
    }

    /* Spawn background sender thread so 1 MB streams concurrently */
    LargeSenderArg s_arg = { .fd = sv[0], .buf = orig_buf, .len = payload_size };
    pthread_t tid;
    assert(pthread_create(&tid, NULL, large_sender_thread, &s_arg) == 0);

    FrameHeader hdr;
    int rc = recv_frame_header(sv[1], &hdr);
    assert(rc == 0);
    assert(hdr.type == MSG_FILE_DATA);
    assert(hdr.payload_len == payload_size);

    void *recvd_buf = NULL;
    rc = recv_frame_payload_alloc(sv[1], &hdr, &recvd_buf);
    assert(rc == 0);
    assert(recvd_buf != NULL);

    /* Verify bit-exact integrity */
    assert(memcmp(orig_buf, recvd_buf, payload_size) == 0);

    assert(pthread_join(tid, NULL) == 0);

    free(orig_buf);
    free(recvd_buf);
    close(sv[0]);
    close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Test 7: Partial I/O & Fragmented Socket Delivery
 * ---------------------------------------------------------------------------- */
static void test_partial_io_fragmentation(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        TEST_FAIL("test_partial_io_fragmentation", "socketpair failed");
    }

    /* Manually craft a 16-byte header and split transmission into 3-byte chunks */
    FrameHeader wire_hdr;
    wire_hdr.magic       = htons(PROTOCOL_MAGIC);
    wire_hdr.version     = PROTOCOL_VERSION;
    wire_hdr.type        = MSG_GET;
    wire_hdr.flags       = htons(FLAG_NONE);
    wire_hdr.reserved    = htons(0x0000);
    wire_hdr.payload_len = proto_hton64(10);

    const char payload_data[10] = "0123456789";

    /* Total message size = 16 (hdr) + 10 (data) = 26 bytes */
    char raw_stream[26];
    memcpy(raw_stream, &wire_hdr, 16);
    memcpy(raw_stream + 16, payload_data, 10);

    /* Send stream in irregular fragments: 3, 5, 2, 7, 9 bytes */
    size_t chunk_sizes[] = {3, 5, 2, 7, 9};
    size_t offset = 0;
    for (size_t i = 0; i < 5; i++) {
        ssize_t written = write(sv[0], raw_stream + offset, chunk_sizes[i]);
        assert(written == (ssize_t)chunk_sizes[i]);
        offset += chunk_sizes[i];
    }
    assert(offset == 26);

    /* Receiver should cleanly reconstruct header despite fragmentation */
    FrameHeader decoded_hdr;
    int rc = recv_frame_header(sv[1], &decoded_hdr);
    assert(rc == 0);
    assert(decoded_hdr.type == MSG_GET);
    assert(decoded_hdr.payload_len == 10);

    char recvd_payload[11];
    memset(recvd_payload, 0, sizeof(recvd_payload));
    rc = recv_frame_payload(sv[1], &decoded_hdr, recvd_payload, sizeof(recvd_payload));
    assert(rc == 0);
    assert(memcmp(recvd_payload, payload_data, 10) == 0);

    close(sv[0]);
    close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Test 8: Orderly Remote EOF Before Frame Header (Returns 1)
 * ---------------------------------------------------------------------------- */
static void test_clean_eof(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        TEST_FAIL("test_clean_eof", "socketpair failed");
    }

    /* Close the writer side cleanly before sending anything */
    close(sv[0]);

    FrameHeader hdr;
    int rc = recv_frame_header(sv[1], &hdr);
    /* Clean EOF before any bytes read must return 1 */
    assert(rc == 1);

    close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Test 9: Truncated Mid-Header EOF (Returns -1)
 * ---------------------------------------------------------------------------- */
static void test_truncated_header_eof(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        TEST_FAIL("test_truncated_header_eof", "socketpair failed");
    }

    /* Write only 7 bytes of a 16-byte header, then close abruptly */
    char partial_hdr[7] = {0x54, 0x46, 0x01, 0x01, 0x00, 0x00, 0x00};
    ssize_t n = write(sv[0], partial_hdr, sizeof(partial_hdr));
    assert(n == 7);
    close(sv[0]);

    FrameHeader hdr;
    int rc = recv_frame_header(sv[1], &hdr);
    /* Truncated header must return -1 (error) */
    assert(rc == -1);

    close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Test 10: Truncated Mid-Payload EOF (Returns -1)
 * ---------------------------------------------------------------------------- */
static void test_truncated_payload_eof(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        TEST_FAIL("test_truncated_payload_eof", "socketpair failed");
    }

    /* Header claims 100 bytes payload */
    FrameHeader wire_hdr;
    wire_hdr.magic       = htons(PROTOCOL_MAGIC);
    wire_hdr.version     = PROTOCOL_VERSION;
    wire_hdr.type        = MSG_FILE_DATA;
    wire_hdr.flags       = htons(FLAG_NONE);
    wire_hdr.reserved    = htons(0x0000);
    wire_hdr.payload_len = proto_hton64(100);

    /* Write 16-byte header */
    assert(write(sv[0], &wire_hdr, 16) == 16);

    /* Write only 20 bytes of payload, then close */
    char short_buf[20] = {0};
    assert(write(sv[0], short_buf, 20) == 20);
    close(sv[0]);

    FrameHeader hdr;
    int rc = recv_frame_header(sv[1], &hdr);
    assert(rc == 0);
    assert(hdr.payload_len == 100);

    char dest_buf[100];
    rc = recv_frame_payload(sv[1], &hdr, dest_buf, sizeof(dest_buf));
    /* Truncated payload read must return -1 */
    assert(rc == -1);

    close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Test 11: Malformed Header Validations (Magic, Version, Reserved)
 * ---------------------------------------------------------------------------- */
static void test_malformed_headers(void) {
    int sv[2];

    /* Case A: Magic Mismatch */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) TEST_FAIL("test_malformed_headers", "socketpair");
    FrameHeader bad_magic;
    bad_magic.magic = htons(0xDEAD);
    bad_magic.version = PROTOCOL_VERSION;
    bad_magic.type = MSG_GET;
    bad_magic.flags = 0;
    bad_magic.reserved = 0;
    bad_magic.payload_len = 0;
    assert(write(sv[0], &bad_magic, 16) == 16);
    FrameHeader hdr;
    assert(recv_frame_header(sv[1], &hdr) == -1);
    close(sv[0]); close(sv[1]);

    /* Case B: Version Mismatch */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) TEST_FAIL("test_malformed_headers", "socketpair");
    FrameHeader bad_version;
    bad_version.magic = htons(PROTOCOL_MAGIC);
    bad_version.version = 0x99; /* Unsupported version */
    bad_version.type = MSG_GET;
    bad_version.flags = 0;
    bad_version.reserved = 0;
    bad_version.payload_len = 0;
    assert(write(sv[0], &bad_version, 16) == 16);
    assert(recv_frame_header(sv[1], &hdr) == -1);
    close(sv[0]); close(sv[1]);

    /* Case C: Non-Zero Reserved */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) TEST_FAIL("test_malformed_headers", "socketpair");
    FrameHeader bad_reserved;
    bad_reserved.magic = htons(PROTOCOL_MAGIC);
    bad_reserved.version = PROTOCOL_VERSION;
    bad_reserved.type = MSG_GET;
    bad_reserved.flags = 0;
    bad_reserved.reserved = htons(0x0001); /* Non-zero */
    bad_reserved.payload_len = 0;
    assert(write(sv[0], &bad_reserved, 16) == 16);
    assert(recv_frame_header(sv[1], &hdr) == -1);
    close(sv[0]); close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Test 12: Payload Limit Violations (MAX_PAYLOAD_SIZE and Buffer Overflows)
 * ---------------------------------------------------------------------------- */
static void test_payload_limits(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        TEST_FAIL("test_payload_limits", "socketpair failed");
    }

    /* send_frame with payload_len > MAX_PAYLOAD_SIZE should immediately fail */
    int rc = send_frame(sv[0], MSG_FILE_DATA, FLAG_NONE, "x", (uint64_t)MAX_PAYLOAD_SIZE + 1);
    assert(rc == -1);
    assert(errno == EMSGSIZE);

    /* Manually craft header with payload_len > MAX_PAYLOAD_SIZE */
    FrameHeader giant_hdr;
    giant_hdr.magic       = htons(PROTOCOL_MAGIC);
    giant_hdr.version     = PROTOCOL_VERSION;
    giant_hdr.type        = MSG_FILE_DATA;
    giant_hdr.flags       = htons(FLAG_NONE);
    giant_hdr.reserved    = htons(0x0000);
    giant_hdr.payload_len = proto_hton64((uint64_t)MAX_PAYLOAD_SIZE + 1024);

    assert(write(sv[0], &giant_hdr, 16) == 16);

    FrameHeader decoded;
    /* recv_frame_header should reject payload exceeding MAX_PAYLOAD_SIZE */
    assert(recv_frame_header(sv[1], &decoded) == -1);
    assert(errno == EMSGSIZE);

    close(sv[0]);
    close(sv[1]);

    /* Test destination buffer capacity overflow in recv_frame_payload */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) TEST_FAIL("test_payload_limits", "socketpair");
    FrameHeader hdr_valid;
    hdr_valid.magic = PROTOCOL_MAGIC;
    hdr_valid.version = PROTOCOL_VERSION;
    hdr_valid.type = MSG_FILE_DATA;
    hdr_valid.flags = 0;
    hdr_valid.reserved = 0;
    hdr_valid.payload_len = 200; /* Declared 200 bytes */

    char small_buffer[50]; /* Caller only provides 50 bytes */
    /* Must return -1 with ENOBUFS */
    assert(recv_frame_payload(sv[1], &hdr_valid, small_buffer, sizeof(small_buffer)) == -1);
    assert(errno == ENOBUFS);

    close(sv[0]);
    close(sv[1]);
}

/* ----------------------------------------------------------------------------
 * Main Entry Point
 * ---------------------------------------------------------------------------- */
int main(void) {
    /* Disable stdout buffering for real-time console and pipe visibility */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("===============================================================\n");
    printf("IT305 Protocol Unit Test Suite (common/protocol.*)\n");
    printf("===============================================================\n\n");

    run_test("Static Wire Layout & Struct Packing", test_wire_layout);
    run_test("Multi-Byte Endianness & Struct Serialization", test_endianness);
    run_test("Message Type Registry Validation", test_message_types);
    run_test("Standard Frame Round-Trip (MSG_GET)", test_standard_frame_roundtrip);
    run_test("Empty Payload Frame (MSG_TRANSFER_END)", test_empty_payload_frame);
    run_test("Large Frame (1 MB) Concurrent Streaming & Dynamic Allocation", test_large_frame);
    run_test("Partial I/O & Fragmented Socket Stream Reassembly", test_partial_io_fragmentation);
    run_test("Orderly Remote EOF Before Frame (Return 1)", test_clean_eof);
    run_test("Truncated Mid-Header Premature EOF (Return -1)", test_truncated_header_eof);
    run_test("Truncated Mid-Payload Premature EOF (Return -1)", test_truncated_payload_eof);
    run_test("Malformed Header Validation (Magic, Version, Reserved)", test_malformed_headers);
    run_test("Payload Limits & Buffer Capacity Protection", test_payload_limits);

    printf("\n===============================================================\n");
    printf("Test Results: %d/%d Passed (100%% Success Rate)\n", passed_tests, total_tests);
    printf("Status: common/protocol.* Fully Verified and Gate 1.1 Ready\n");
    printf("===============================================================\n");

    return 0;
}
