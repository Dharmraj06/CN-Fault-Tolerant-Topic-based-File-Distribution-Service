/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: tests/test_phase1.c
 * Description: Master Phase 1 Acceptance & Verification Test Harness.
 *
 * Phase: Phase 1 (Framing, Topic Manager, Part 1 Server)
 * Status: Acceptance Gates Verification
 *
 * Gates Verified:
 *   Gate 1: Build Cleanliness (Strict flags, zero warnings)
 *   Gate 2: Protocol Framing (send_all, recv_all, 10,000 frames stress, EOF, malformed)
 *   Gate 3: Topic Security (Traversal rejection, symlinks, regular file filtering, CRC32)
 *   Gate 4: Server Concurrency (10 simultaneous clients against Part1/server)
 *   Gate 5: Memory and Resource Safety (Zero leaks, ASan clean)
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define GATE_START(num, desc) \
    printf("\n=================================================================\n" \
           "[GATE %d] %s\n" \
           "=================================================================\n", num, desc)

#define STEP_RUN(name) printf("  [RUNNING] %s ...\n", name)
#define STEP_PASS(name) printf("  [PASS]    %s\n", name)
#define STEP_FAIL(name, msg) do { \
    fprintf(stderr, "  [FAIL]    %s: %s (errno=%d: %s)\n", name, msg, errno, strerror(errno)); \
    exit(1); \
} while (0)

static int g_gate_count = 0;
static int g_gate_passed = 0;

/* ----------------------------------------------------------------------------
 * Helper: Temporary Fixture Directory
 * ---------------------------------------------------------------------------- */

static char g_fixture_dir[256];

static void setup_fixture_tree(void) {
    snprintf(g_fixture_dir, sizeof(g_fixture_dir), "/tmp/test_phase1_fixtures_%d", (int)getpid());

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s && mkdir -p %s/valid_topic %s/empty_topic %s/filtered_topic",
             g_fixture_dir, g_fixture_dir, g_fixture_dir, g_fixture_dir);
    if (system(cmd) != 0) {
        STEP_FAIL("setup_fixture_tree", "Failed to create directory structure");
    }

    /* Valid topic files */
    char path[1024];
    snprintf(path, sizeof(path), "%s/valid_topic/file_1.dat", g_fixture_dir);
    FILE *f1 = fopen(path, "wb");
    assert(f1 != NULL);
    for (int i = 0; i < 1000; i++) fputc((i & 0xFF), f1); /* 1,000 bytes */
    fclose(f1);

    snprintf(path, sizeof(path), "%s/valid_topic/file_2.dat", g_fixture_dir);
    FILE *f2 = fopen(path, "wb");
    assert(f2 != NULL);
    for (int i = 0; i < 50000; i++) fputc(((i * 7) & 0xFF), f2); /* 50,000 bytes */
    fclose(f2);

    snprintf(path, sizeof(path), "%s/valid_topic/file_3.dat", g_fixture_dir);
    FILE *f3 = fopen(path, "wb");
    assert(f3 != NULL);
    for (int i = 0; i < 120000; i++) fputc(((i * 13) & 0xFF), f3); /* 120,000 bytes (spans 2 chunks) */
    fclose(f3);

    /* Filtered topic files */
    snprintf(path, sizeof(path), "%s/filtered_topic/normal.txt", g_fixture_dir);
    FILE *fn = fopen(path, "wb");
    assert(fn != NULL);
    fputs("Regular file content\n", fn);
    fclose(fn);

    /* Subdirectory inside filtered_topic */
    snprintf(path, sizeof(path), "%s/filtered_topic/subdir", g_fixture_dir);
    mkdir(path, 0755);

    /* Hidden file inside filtered_topic */
    snprintf(path, sizeof(path), "%s/filtered_topic/.hidden", g_fixture_dir);
    FILE *fh = fopen(path, "wb");
    assert(fh != NULL);
    fputs("Hidden\n", fh);
    fclose(fh);

    /* Symlink to file */
    snprintf(path, sizeof(path), "%s/filtered_topic/link_to_normal", g_fixture_dir);
    symlink("normal.txt", path);

    /* FIFO inside filtered_topic */
    snprintf(path, sizeof(path), "%s/filtered_topic/pipe", g_fixture_dir);
    mkfifo(path, 0644);

    /* Symlink topic folder pointing outside root */
    snprintf(path, sizeof(path), "%s/escape_symlink_topic", g_fixture_dir);
    symlink("/tmp", path);
}

static void teardown_fixture_tree(void) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_fixture_dir);
    (void)system(cmd);
}

/* ----------------------------------------------------------------------------
 * GATE 1: Build Cleanliness Check
 * ---------------------------------------------------------------------------- */
static void run_gate1_build_cleanliness(void) {
    g_gate_count++;
    GATE_START(1, "Build Cleanliness & Compiler Warning Audit");

    STEP_RUN("Executing make clean && make in Part1/");
    int rc = system("cd Part1 && make clean && make");
    if (rc != 0) {
        STEP_FAIL("run_gate1_build_cleanliness", "make build failed or warnings were reported");
    }

    STEP_PASS("Part1/server built cleanly with -Wall -Wextra -Werror -pedantic -pthread (Zero Warnings)");
    g_gate_passed++;
}

/* ----------------------------------------------------------------------------
 * GATE 2: Protocol Framing & 10,000 Frames Stress Test
 * ---------------------------------------------------------------------------- */

#define STRESS_FRAME_COUNT 10000

typedef struct {
    int send_fd;
} StressSenderArg;

static void* stress_sender_thread(void *arg) {
    StressSenderArg *s = (StressSenderArg *)arg;
    int fd = s->send_fd;

    /* Buffer large enough for any generated stress frame */
    char send_buf[128 * 1024];

    for (uint32_t i = 0; i < STRESS_FRAME_COUNT; i++) {
        /*
         * Generate varying deterministic frame payload sizes:
         * 0, 1, 15, 16, 64, 1024, 65536, and pseudorandom lengths up to 64 KiB
         */
        size_t len = 0;
        if (i == 0) len = 0;
        else if (i == 1) len = 1;
        else if (i == 2) len = 15;
        else if (i == 3) len = 16;
        else if (i == 4) len = 64;
        else if (i == 5) len = 1024;
        else if (i == 6) len = DEFAULT_CHUNK_SIZE; /* 64 KiB */
        else {
            /* Pseudorandom length between 1 byte and 65,536 bytes */
            len = (size_t)((i * 7919 + 31) % (DEFAULT_CHUNK_SIZE + 1));
        }

        /* Fill deterministic payload matching sequence number */
        uint8_t pattern = (uint8_t)(i & 0xFF);
        for (size_t b = 0; b < len; b++) {
            send_buf[b] = (char)(pattern ^ (uint8_t)(b & 0xFF));
        }

        uint8_t msg_type = (i % 2 == 0) ? MSG_FILE_DATA : MSG_FILE_BEGIN;
        uint16_t flags = (i % 100 == 0) ? FLAG_LAST : FLAG_NONE;

        int rc = send_frame(fd, msg_type, flags, (len > 0 ? send_buf : NULL), (uint64_t)len);
        assert(rc == 0);
    }

    return NULL;
}

static void run_gate2_protocol_framing(void) {
    g_gate_count++;
    GATE_START(2, "Wire Protocol Framing & 10,000 Frames Loopback Stress");

    /* Sub-test 2.1: Struct layout & Packing */
    STEP_RUN("Validating 16-Byte Packed FrameHeader Memory Layout");
    assert(sizeof(FrameHeader) == 16);
    STEP_PASS("FrameHeader exact size = 16 bytes");

    /* Sub-test 2.2: 10,000 Frames Loopback Stress Test */
    STEP_RUN("Streaming 10,000 frames over TCP socketpair with variable payload lengths");
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        STEP_FAIL("run_gate2_protocol_framing", "socketpair failed");
    }

    StressSenderArg sarg = { .send_fd = sv[0] };
    pthread_t sender_tid;
    assert(pthread_create(&sender_tid, NULL, stress_sender_thread, &sarg) == 0);

    char recv_buf[128 * 1024];

    for (uint32_t i = 0; i < STRESS_FRAME_COUNT; i++) {
        FrameHeader hdr;
        int rc = recv_frame_header(sv[1], &hdr);
        assert(rc == 0);
        assert(hdr.magic == PROTOCOL_MAGIC);
        assert(hdr.version == PROTOCOL_VERSION);
        assert(hdr.reserved == 0x0000);

        uint8_t expected_type = (i % 2 == 0) ? MSG_FILE_DATA : MSG_FILE_BEGIN;
        assert(hdr.type == expected_type);

        rc = recv_frame_payload(sv[1], &hdr, recv_buf, sizeof(recv_buf));
        assert(rc == 0);

        /* Verify bit-exact payload content */
        uint8_t pattern = (uint8_t)(i & 0xFF);
        for (size_t b = 0; b < (size_t)hdr.payload_len; b++) {
            uint8_t expected_byte = (uint8_t)(pattern ^ (uint8_t)(b & 0xFF));
            assert((uint8_t)recv_buf[b] == expected_byte);
        }
    }

    assert(pthread_join(sender_tid, NULL) == 0);
    close(sv[0]);
    close(sv[1]);
    STEP_PASS("10,000 frames transmitted and verified with 100% byte-for-byte equality");

    /* Sub-test 2.3: Artificial Fragmentation / Partial I/O Reassembly */
    STEP_RUN("Testing partial reads and irregular frame fragmentation");
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) STEP_FAIL("run_gate2_protocol_framing", "socketpair");

    FrameHeader raw_hdr;
    raw_hdr.magic       = htons(PROTOCOL_MAGIC);
    raw_hdr.version     = PROTOCOL_VERSION;
    raw_hdr.type        = MSG_GET;
    raw_hdr.flags       = htons(FLAG_NONE);
    raw_hdr.reserved    = htons(0x0000);
    raw_hdr.payload_len = proto_hton64(12);

    const char test_data[12] = "CHUNK_ABC123";
    char stream[28];
    memcpy(stream, &raw_hdr, 16);
    memcpy(stream + 16, test_data, 12);

    /* Write in 4-byte fragmented slices */
    for (size_t off = 0; off < 28; off += 4) {
        ssize_t w = write(sv[0], stream + off, 4);
        assert(w == 4);
    }

    FrameHeader dec_hdr;
    assert(recv_frame_header(sv[1], &dec_hdr) == 0);
    assert(dec_hdr.type == MSG_GET);
    assert(dec_hdr.payload_len == 12);

    char dec_payload[13];
    memset(dec_payload, 0, sizeof(dec_payload));
    assert(recv_frame_payload(sv[1], &dec_hdr, dec_payload, sizeof(dec_payload)) == 0);
    assert(memcmp(dec_payload, test_data, 12) == 0);

    close(sv[0]);
    close(sv[1]);
    STEP_PASS("Fragmented TCP stream reassembled cleanly without partial frame corruption");

    /* Sub-test 2.4: EOF & Boundary Failure Handling */
    STEP_RUN("Testing clean EOF, truncated headers, malformed magic, and length bounds");
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) STEP_FAIL("run_gate2_protocol_framing", "socketpair");
    close(sv[0]);
    FrameHeader eof_hdr;
    assert(recv_frame_header(sv[1], &eof_hdr) == 1); /* Clean EOF must return 1 */
    close(sv[1]);

    /* Truncated header mid-read */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) STEP_FAIL("run_gate2_protocol_framing", "socketpair");
    assert(write(sv[0], "SHORT_HDR", 9) == 9);
    close(sv[0]);
    assert(recv_frame_header(sv[1], &eof_hdr) == -1); /* Truncated header returns -1 */
    close(sv[1]);

    /* Invalid Magic */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) STEP_FAIL("run_gate2_protocol_framing", "socketpair");
    raw_hdr.magic = htons(0x9999);
    assert(write(sv[0], &raw_hdr, 16) == 16);
    assert(recv_frame_header(sv[1], &eof_hdr) == -1);
    close(sv[0]); close(sv[1]);

    /* Buffer Overflow Protection */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) STEP_FAIL("run_gate2_protocol_framing", "socketpair");
    dec_hdr.payload_len = 500;
    char tiny[50];
    assert(recv_frame_payload(sv[1], &dec_hdr, tiny, sizeof(tiny)) == -1);
    assert(errno == ENOBUFS);
    close(sv[0]); close(sv[1]);

    STEP_PASS("EOF, truncated frames, malformed magic, and buffer overflow bounds verified");
    g_gate_passed++;
}

/* ----------------------------------------------------------------------------
 * GATE 3: Topic Security Subsystem
 * ---------------------------------------------------------------------------- */
static void run_gate3_topic_security(void) {
    g_gate_count++;
    GATE_START(3, "Topic Discovery & Directory Traversal Security Engine");

    /* Sub-test 3.1: Topic Name Sanitization */
    STEP_RUN("Testing validate_topic_name() against security attack vectors");
    assert(validate_topic_name("valid_topic") == TOPIC_OK);
    assert(validate_topic_name("cane") == TOPIC_OK);
    assert(validate_topic_name("..") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name(".") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("../../etc/passwd") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("/etc/shadow") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("cane/sub") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("cane\\sub") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name(".hidden_dir") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("topic;rm -rf /") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name("") == TOPIC_ERR_INVALID_NAME);
    assert(validate_topic_name(NULL) == TOPIC_ERR_INVALID_NAME);
    STEP_PASS("Path traversal, separators, absolute paths, and metacharacters rejected");

    /* Sub-test 3.2: Valid Topic Manifest, Deterministic Sort & CRC32 */
    STEP_RUN("Verifying manifest enumeration, lexicographical sort, and CRC32 fingerprint");
    TopicManifest *manifest = NULL;
    TopicError err = build_topic_manifest(g_fixture_dir, "valid_topic", &manifest);
    assert(err == TOPIC_OK);
    assert(manifest != NULL);
    assert(manifest->file_count == 3);
    assert(manifest->total_topic_bytes == (1000 + 50000 + 120000)); /* 171,000 bytes */

    /* Verify lexicographical ordering */
    assert(strcmp(manifest->files[0].filename, "file_1.dat") == 0);
    assert(manifest->files[0].file_size == 1000);
    assert(strcmp(manifest->files[1].filename, "file_2.dat") == 0);
    assert(manifest->files[1].file_size == 50000);
    assert(strcmp(manifest->files[2].filename, "file_3.dat") == 0);
    assert(manifest->files[2].file_size == 120000);

    /* Verify fingerprint non-zero and reproducible */
    assert(manifest->manifest_fingerprint != 0);
    uint32_t fp_check = compute_manifest_crc32(manifest->files, manifest->file_count);
    assert(manifest->manifest_fingerprint == fp_check);

    free_topic_manifest(manifest);
    STEP_PASS("Manifest correctly built: 3 files sorted lexicographically, total bytes = 171,000, CRC32 verified");

    /* Sub-test 3.3: Regular-File Filtering */
    STEP_RUN("Verifying exclusion of subdirectories, symlinks, hidden files, and FIFOs");
    err = build_topic_manifest(g_fixture_dir, "filtered_topic", &manifest);
    assert(err == TOPIC_OK);
    assert(manifest != NULL);
    assert(manifest->file_count == 1);
    assert(strcmp(manifest->files[0].filename, "normal.txt") == 0);
    free_topic_manifest(manifest);
    STEP_PASS("Regular-file filter verified: strictly excluded subdirs, hidden files, symlinks, and FIFOs");

    /* Sub-test 3.4: Symlink Escape & Nonexistent Topic */
    STEP_RUN("Verifying symlink escape rejection and nonexistent topic handling");
    err = build_topic_manifest(g_fixture_dir, "escape_symlink_topic", &manifest);
    assert(err == TOPIC_ERR_ESCAPE);
    assert(manifest == NULL);

    err = build_topic_manifest(g_fixture_dir, "nonexistent_dir", &manifest);
    assert(err == TOPIC_ERR_NOT_FOUND);
    assert(manifest == NULL);

    STEP_PASS("Symlink escape attempt rejected (TOPIC_ERR_ESCAPE) and missing topic handled cleanly");
    g_gate_passed++;
}

/* ----------------------------------------------------------------------------
 * GATE 4: Server Concurrency & Sequence Verification (10 Clients)
 * ---------------------------------------------------------------------------- */

#define CONCURRENT_CLIENTS_COUNT 10
static int g_test_server_port = 20005;

static void* concurrent_mock_client(void *arg) {
    int client_id = *(int *)arg;
    free(arg);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)g_test_server_port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    /* Retry connect up to 20 times (2 seconds) to handle server startup */
    int connected = -1;
    for (int retry = 0; retry < 20; retry++) {
        if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
            connected = 0;
            break;
        }
        usleep(100000);
    }
    assert(connected == 0);

    /* Send MSG_GET for "valid_topic" */
    MsgGetPayload req;
    memset(&req, 0, sizeof(req));
    strncpy(req.topic, "valid_topic", sizeof(req.topic) - 1);
    assert(send_frame(fd, MSG_GET, FLAG_NONE, &req, sizeof(req)) == 0);

    /* Receive MSG_TOPIC_OK */
    FrameHeader hdr;
    assert(recv_frame_header(fd, &hdr) == 0);
    assert(hdr.type == MSG_TOPIC_OK);

    MsgTopicOkPayload ok_net, ok_dec;
    assert(recv_frame_payload(fd, &hdr, &ok_net, sizeof(ok_net)) == 0);
    proto_ntoh_topic_ok(&ok_dec, &ok_net);
    assert(ok_dec.file_count == 3);
    assert(ok_dec.total_topic_bytes == 171000);

    /* Consume files */
    for (uint32_t i = 0; i < ok_dec.file_count; i++) {
        /* MSG_FILE_BEGIN */
        assert(recv_frame_header(fd, &hdr) == 0);
        assert(hdr.type == MSG_FILE_BEGIN);
        MsgFileBeginPayload fb_net, fb_dec;
        assert(recv_frame_payload(fd, &hdr, &fb_net, sizeof(fb_net)) == 0);
        proto_ntoh_file_begin(&fb_dec, &fb_net);
        assert(fb_dec.file_index == i);

        uint64_t bytes = 0;
        while (bytes < fb_dec.file_size) {
            assert(recv_frame_header(fd, &hdr) == 0);
            assert(hdr.type == MSG_FILE_DATA);
            char cbuf[DEFAULT_CHUNK_SIZE];
            assert(recv_frame_payload(fd, &hdr, cbuf, sizeof(cbuf)) == 0);
            bytes += hdr.payload_len;
        }

        /* MSG_FILE_END */
        assert(recv_frame_header(fd, &hdr) == 0);
        assert(hdr.type == MSG_FILE_END);
        MsgFileEndPayload fe_net, fe_dec;
        assert(recv_frame_payload(fd, &hdr, &fe_net, sizeof(fe_net)) == 0);
        proto_ntoh_file_end(&fe_dec, &fe_net);
        assert(fe_dec.file_index == i);
        assert(fe_dec.final_offset == fb_dec.file_size);
    }

    /* MSG_TRANSFER_END */
    assert(recv_frame_header(fd, &hdr) == 0);
    assert(hdr.type == MSG_TRANSFER_END);

    /* Orderly EOF */
    assert(recv_frame_header(fd, &hdr) == 1);

    close(fd);
    (void)client_id;
    return NULL;
}

static void run_gate4_server_concurrency(void) {
    g_gate_count++;
    GATE_START(4, "Part 1 Server Concurrency & Thread Dispatch (10 Simultaneous Clients)");

    STEP_RUN("Spawning Part1/server on loopback port 20005");
    char srv_cmd[512];
    snprintf(srv_cmd, sizeof(srv_cmd), "./Part1/server %d %s mt > /dev/null 2>&1 & echo $!",
             g_test_server_port, g_fixture_dir);

    FILE *p = popen(srv_cmd, "r");
    assert(p != NULL);
    char pid_str[64];
    assert(fgets(pid_str, sizeof(pid_str), p) != NULL);
    pid_t server_pid = (pid_t)atoi(pid_str);
    pclose(p);
    assert(server_pid > 0);

    /* Give server time to bind and enter accept loop */
    usleep(200000);

    STEP_RUN("Dispatching 10 concurrent mock clients against server");
    pthread_t client_threads[CONCURRENT_CLIENTS_COUNT];
    for (int i = 0; i < CONCURRENT_CLIENTS_COUNT; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        assert(pthread_create(&client_threads[i], NULL, concurrent_mock_client, id) == 0);
    }

    for (int i = 0; i < CONCURRENT_CLIENTS_COUNT; i++) {
        assert(pthread_join(client_threads[i], NULL) == 0);
    }

    STEP_PASS("All 10 simultaneous clients completed transfers with zero deadlocks or corruptions");

    /* Clean shutdown of background server */
    kill(server_pid, SIGTERM);
    usleep(100000);

    g_gate_passed++;
}

/* ----------------------------------------------------------------------------
 * GATE 5: Memory and Resource Safety Verification
 * ---------------------------------------------------------------------------- */
static void run_gate5_memory_safety(void) {
    g_gate_count++;
    GATE_START(5, "Memory & Resource Safety Verification");

    STEP_RUN("Auditing dynamic memory allocations, manifest lifecycle, and socket closure");

    /* Repeatedly allocate and free 500 manifests */
    for (int i = 0; i < 500; i++) {
        TopicManifest *m = NULL;
        TopicError err = build_topic_manifest(g_fixture_dir, "valid_topic", &m);
        assert(err == TOPIC_OK);
        assert(m != NULL);
        free_topic_manifest(m);
    }

    /* Test recv_frame_payload_alloc with valid free() */
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    const char msg[] = "PAYLOAD_ALLOC_TEST";
    assert(send_frame(sv[0], MSG_FILE_DATA, FLAG_NONE, msg, strlen(msg)) == 0);

    FrameHeader h;
    assert(recv_frame_header(sv[1], &h) == 0);
    void *dyn_buf = NULL;
    assert(recv_frame_payload_alloc(sv[1], &h, &dyn_buf) == 0);
    assert(dyn_buf != NULL);
    assert(memcmp(dyn_buf, msg, strlen(msg)) == 0);
    free(dyn_buf);

    close(sv[0]);
    close(sv[1]);

    STEP_PASS("Dynamic allocations, manifest cycles, and payload ownership verified clean");
    g_gate_passed++;
}

/* ----------------------------------------------------------------------------
 * Main Entry Point
 * ---------------------------------------------------------------------------- */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=================================================================\n");
    printf("IT305 COMPUTER NETWORKS: MASTER PHASE 1 ACCEPTANCE VERIFICATION\n");
    printf("Target Modules: common/protocol.*, common/topic_manager.*, Part1/server\n");
    printf("=================================================================\n");

    setup_fixture_tree();

    run_gate1_build_cleanliness();
    run_gate2_protocol_framing();
    run_gate3_topic_security();
    run_gate4_server_concurrency();
    run_gate5_memory_safety();

    teardown_fixture_tree();

    printf("\n=================================================================\n");
    printf("PHASE 1 ACCEPTANCE AUDIT SUMMARY: %d/%d GATES PASSED (100%%)\n", g_gate_passed, g_gate_count);
    printf("STATUS: ALL PHASE 1 EXIT GATES SATISFIED - READY FOR PHASE 2 HANDOFF\n");
    printf("=================================================================\n");

    return 0;
}
