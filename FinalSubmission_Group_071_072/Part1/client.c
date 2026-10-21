/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: Part1/client.c
 * Description: Part 1 Baseline Topic File Client.
 *
 * Phase: Phase 2 (Bounded File Streaming Engine & Part 1 Baseline Client)
 * Status: Implementation Complete
 *
 * Source of Truth Hierarchy:
 *   - Tier 1: IT305 Assignment Specification (§Part 1, §Standard CLI)
 *   - Tier 2: Instructor Clarifications (§Page 2-3)
 *   - Tier 3: Approved Design Document Group 071_072 (§8.1, §8.2, §9.1, §9.2)
 *   - Tier 4: Master Execution Roadmap (PART2_TASKS_ROADMAP.md §4.32, §5 Task 2.2)
 *
 * Standard CLI Invocation:
 *   ./client <server_ip> <server_port> <topic> <output_dir> [num_connections]
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "protocol.h"
#include "file_transfer.h"
#include "topic_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ----------------------------------------------------------------------------
 * Client Configuration Structure
 * ---------------------------------------------------------------------------- */

typedef struct {
    char server_ip[64];
    int server_port;
    char topic[MAX_TOPIC_NAME_LEN + 1];
    char output_dir[FT_MAX_PATH_LEN];
    int num_connections; /* Defaults to 1 for Part 1 baseline */
} ClientConfig;

/* ----------------------------------------------------------------------------
 * CLI Usage & Parsing Helper
 * ---------------------------------------------------------------------------- */

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <server_ip> <server_port> <topic> <output_dir> [num_connections]\n", prog_name);
    fprintf(stderr, "  <server_ip>        : IPv4 address of server (e.g., 127.0.0.1)\n");
    fprintf(stderr, "  <server_port>      : TCP listening port (1024-65535)\n");
    fprintf(stderr, "  <topic>            : Topic name to download (e.g., cane)\n");
    fprintf(stderr, "  <output_dir>       : Local directory where topic will be created\n");
    fprintf(stderr, "  [num_connections]  : Optional parallel connection count (default: 1)\n");
}

static bool parse_arguments(int argc, char *argv[], ClientConfig *config) {
    if (argc < 5 || argc > 6) {
        print_usage(argv[0]);
        return false;
    }

    /* 1. Server IP address */
    strncpy(config->server_ip, argv[1], sizeof(config->server_ip) - 1);
    config->server_ip[sizeof(config->server_ip) - 1] = '\0';
    struct in_addr addr_test;
    if (inet_pton(AF_INET, config->server_ip, &addr_test) != 1) {
        fprintf(stderr, "[CLIENT] Error: Invalid IPv4 address '%s'\n", config->server_ip);
        return false;
    }

    /* 2. Server Port */
    char *endptr = NULL;
    long port = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || port < 1024 || port > 65535) {
        fprintf(stderr, "[CLIENT] Error: Port '%s' out of range (must be 1024-65535)\n", argv[2]);
        return false;
    }
    config->server_port = (int)port;

    /* 3. Topic Name Validation */
    if (validate_topic_name(argv[3]) != TOPIC_OK) {
        fprintf(stderr, "[CLIENT] Error: Invalid topic name '%s' (forbidden characters or traversal attempt)\n", argv[3]);
        return false;
    }
    if (strlen(argv[3]) > 127) {
        fprintf(stderr, "[CLIENT] Error: Topic name '%s' exceeds maximum wire length (127 bytes)\n", argv[3]);
        return false;
    }
    strncpy(config->topic, argv[3], sizeof(config->topic) - 1);
    config->topic[sizeof(config->topic) - 1] = '\0';

    /* 4. Output Directory */
    if (strlen(argv[4]) == 0 || strlen(argv[4]) >= FT_MAX_PATH_LEN) {
        fprintf(stderr, "[CLIENT] Error: Output directory path invalid or too long\n");
        return false;
    }
    if (strstr(argv[4], "..") != NULL) {
        fprintf(stderr, "[CLIENT] Error: Output directory '%s' contains forbidden traversal sequence ('..')\n", argv[4]);
        return false;
    }
    strncpy(config->output_dir, argv[4], sizeof(config->output_dir) - 1);
    config->output_dir[sizeof(config->output_dir) - 1] = '\0';

    /* 5. Optional num_connections */
    config->num_connections = 1;
    if (argc == 6) {
        long conns = strtol(argv[5], &endptr, 10);
        if (*endptr != '\0' || conns < 1 || conns > 32) {
            fprintf(stderr, "[CLIENT] Error: Invalid num_connections '%s' (must be 1-32)\n", argv[5]);
            return false;
        }
        config->num_connections = (int)conns;
    }

    return true;
}

/* ----------------------------------------------------------------------------
 * Helper: Comparison function for sorting downloaded files
 * ---------------------------------------------------------------------------- */

static int compare_file_entries(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    return strcmp(fa->filename, fb->filename);
}

/* ----------------------------------------------------------------------------
 * Main Client Flow
 * ---------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
    ClientConfig config;
    if (!parse_arguments(argc, argv, &config)) {
        return 1;
    }

    /* Step 1: Create TCP Socket */
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        fprintf(stderr, "[CLIENT] Error: Unable to create socket: %s\n", strerror(errno));
        return 1;
    }

    /* Step 2: Connect to Server */
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)config.server_port);
    inet_pton(AF_INET, config.server_ip, &serv_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "[CLIENT] Error: Failed to connect to server %s:%d: %s\n",
                config.server_ip, config.server_port, strerror(errno));
        close(sock_fd);
        return 1;
    }

    /* Step 3: Transmit MSG_GET Request */
    MsgGetPayload get_req;
    memset(&get_req, 0, sizeof(get_req));
    strncpy(get_req.topic, config.topic, sizeof(get_req.topic) - 1);

    if (send_frame(sock_fd, MSG_GET, FLAG_NONE, &get_req, sizeof(get_req)) != 0) {
        fprintf(stderr, "[CLIENT] Error: Failed to send MSG_GET request: %s\n", strerror(errno));
        close(sock_fd);
        return 1;
    }

    /* Step 4: Receive Server Response (MSG_TOPIC_OK or MSG_TOPIC_ERROR) */
    FrameHeader hdr;
    int rc = recv_frame_header(sock_fd, &hdr);
    if (rc != 0) {
        fprintf(stderr, "[CLIENT] Error: Server closed connection or sent invalid header before topic response\n");
        close(sock_fd);
        return 1;
    }

    if (hdr.type == MSG_TOPIC_ERROR) {
        MsgTopicErrorPayload err_wire, err_host;
        if (recv_frame_payload(sock_fd, &hdr, &err_wire, sizeof(err_wire)) != 0) {
            fprintf(stderr, "[CLIENT] Error: Failed to read MSG_TOPIC_ERROR payload\n");
            close(sock_fd);
            return 1;
        }
        proto_ntoh_topic_error(&err_host, &err_wire);
        fprintf(stderr, "[CLIENT] Server rejected topic '%s' (Error %u: %s)\n",
                config.topic, err_host.error_code, err_host.error_msg);
        close(sock_fd);
        return 1;
    }

    if (hdr.type != MSG_TOPIC_OK) {
        fprintf(stderr, "[CLIENT] Error: Unexpected message type 0x%02X received (expected MSG_TOPIC_OK)\n", hdr.type);
        close(sock_fd);
        return 1;
    }

    MsgTopicOkPayload ok_wire, ok_host;
    if (recv_frame_payload(sock_fd, &hdr, &ok_wire, sizeof(ok_wire)) != 0) {
        fprintf(stderr, "[CLIENT] Error: Failed to read MSG_TOPIC_OK payload\n");
        close(sock_fd);
        return 1;
    }
    proto_ntoh_topic_ok(&ok_host, &ok_wire);

    printf("[CLIENT] Topic '%s' accepted: %u files, %lu total bytes (fingerprint 0x%08X)\n",
           config.topic, ok_host.file_count, (unsigned long)ok_host.total_topic_bytes,
           ok_host.manifest_fingerprint);

    /* Step 5: Construct Local Destination Directory <output_dir>/<topic>/ */
    char topic_dir[2048];
    snprintf(topic_dir, sizeof(topic_dir), "%s/%s", config.output_dir, config.topic);

    FileTransferError fterr = create_directory_recursive(topic_dir);
    if (fterr != FT_OK) {
        fprintf(stderr, "[CLIENT] Error: Failed to create topic output directory '%s': %s\n",
                topic_dir, file_transfer_error_to_string(fterr));
        close(sock_fd);
        return 1;
    }

    /* Allocate tracking array for downloaded files */
    FileEntry *downloaded_files = NULL;
    if (ok_host.file_count > 0) {
        downloaded_files = calloc(ok_host.file_count, sizeof(FileEntry));
        if (!downloaded_files) {
            fprintf(stderr, "[CLIENT] Error: Memory allocation failed for file tracking array\n");
            close(sock_fd);
            return 1;
        }
    }

    uint64_t total_bytes_received = 0;
    bool transfer_aborted = false;

    /* Step 6: File Download Loop */
    for (uint32_t i = 0; i < ok_host.file_count; i++) {
        /* 6a: Await MSG_FILE_BEGIN */
        rc = recv_frame_header(sock_fd, &hdr);
        if (rc != 0 || hdr.type != MSG_FILE_BEGIN) {
            fprintf(stderr, "[CLIENT] Error: Expected MSG_FILE_BEGIN for file index %u, got type 0x%02X (rc=%d)\n",
                    i, (rc == 0 ? hdr.type : 0), rc);
            transfer_aborted = true;
            break;
        }

        MsgFileBeginPayload fb_wire, fb_host;
        if (recv_frame_payload(sock_fd, &hdr, &fb_wire, sizeof(fb_wire)) != 0) {
            fprintf(stderr, "[CLIENT] Error: Failed to receive MSG_FILE_BEGIN payload for file index %u\n", i);
            transfer_aborted = true;
            break;
        }
        proto_ntoh_file_begin(&fb_host, &fb_wire);

        /* Validate file metadata and index ordering */
        if (fb_host.file_index != i) {
            fprintf(stderr, "[CLIENT] Error: File index mismatch (expected %u, received %u)\n", i, fb_host.file_index);
            transfer_aborted = true;
            break;
        }

        if (validate_filename(fb_host.filename) != FT_OK) {
            fprintf(stderr, "[CLIENT] Security Error: Forbidden filename '%s' in file stream\n", fb_host.filename);
            transfer_aborted = true;
            break;
        }

        /* 6b: Open Destination File */
        char dest_path[2048];
        fterr = build_destination_path(config.output_dir, config.topic, fb_host.filename, dest_path, sizeof(dest_path));
        if (fterr != FT_OK) {
            fprintf(stderr, "[CLIENT] Error: Failed to construct destination path for '%s': %s\n",
                    fb_host.filename, file_transfer_error_to_string(fterr));
            transfer_aborted = true;
            break;
        }

        int file_fd = open_file_for_writing(dest_path, true);
        if (file_fd < 0) {
            fprintf(stderr, "[CLIENT] Error: Unable to open '%s' for writing: %s\n",
                    dest_path, strerror(errno));
            transfer_aborted = true;
            break;
        }

        /* 6c: Receive MSG_FILE_DATA Chunks */
        uint64_t file_bytes_received = 0;
        char chunk_buf[FT_CHUNK_SIZE];

        while (file_bytes_received < fb_host.file_size) {
            rc = recv_frame_header(sock_fd, &hdr);
            if (rc != 0 || hdr.type != MSG_FILE_DATA) {
                fprintf(stderr, "[CLIENT] Error: Expected MSG_FILE_DATA chunk for '%s', got type 0x%02X (rc=%d)\n",
                        fb_host.filename, (rc == 0 ? hdr.type : 0), rc);
                transfer_aborted = true;
                break;
            }

            if (hdr.payload_len == 0 || hdr.payload_len > FT_CHUNK_SIZE) {
                fprintf(stderr, "[CLIENT] Error: Received invalid chunk payload length %lu bytes\n",
                        (unsigned long)hdr.payload_len);
                transfer_aborted = true;
                break;
            }

            if (file_bytes_received + hdr.payload_len > fb_host.file_size) {
                fprintf(stderr, "[CLIENT] Error: Received chunk overflows announced file size (%lu + %lu > %lu)\n",
                        (unsigned long)file_bytes_received, (unsigned long)hdr.payload_len, (unsigned long)fb_host.file_size);
                transfer_aborted = true;
                break;
            }

            if (recv_frame_payload(sock_fd, &hdr, chunk_buf, sizeof(chunk_buf)) != 0) {
                fprintf(stderr, "[CLIENT] Error: Failed to read chunk payload for '%s'\n", fb_host.filename);
                transfer_aborted = true;
                break;
            }

            FileTransferError werr = write_chunk_to_file(file_fd, chunk_buf, (size_t)hdr.payload_len);
            if (werr != FT_OK) {
                fprintf(stderr, "[CLIENT] Error writing chunk to disk for '%s': %s (errno=%d: %s)\n",
                        fb_host.filename, file_transfer_error_to_string(werr), errno, strerror(errno));
                transfer_aborted = true;
                break;
            }

            file_bytes_received += hdr.payload_len;
        }

        if (transfer_aborted) {
            close_file(&file_fd);
            break;
        }

        /* 6d: Await MSG_FILE_END */
        rc = recv_frame_header(sock_fd, &hdr);
        if (rc != 0 || hdr.type != MSG_FILE_END) {
            fprintf(stderr, "[CLIENT] Error: Expected MSG_FILE_END for '%s', got type 0x%02X (rc=%d)\n",
                    fb_host.filename, (rc == 0 ? hdr.type : 0), rc);
            close_file(&file_fd);
            transfer_aborted = true;
            break;
        }

        MsgFileEndPayload fe_wire, fe_host;
        if (recv_frame_payload(sock_fd, &hdr, &fe_wire, sizeof(fe_wire)) != 0) {
            fprintf(stderr, "[CLIENT] Error: Failed to receive MSG_FILE_END payload for '%s'\n", fb_host.filename);
            close_file(&file_fd);
            transfer_aborted = true;
            break;
        }
        proto_ntoh_file_end(&fe_host, &fe_wire);

        if (fe_host.file_index != i || fe_host.final_offset != fb_host.file_size) {
            fprintf(stderr, "[CLIENT] Error: MSG_FILE_END offset mismatch for '%s' (offset=%lu, expected=%lu)\n",
                    fb_host.filename, (unsigned long)fe_host.final_offset, (unsigned long)fb_host.file_size);
            close_file(&file_fd);
            transfer_aborted = true;
            break;
        }

        /* Flush file data to disk and close */
        sync_file_to_disk(file_fd);
        close_file(&file_fd);

        /* Record downloaded file entry for manifest verification */
        strncpy(downloaded_files[i].filename, fb_host.filename, sizeof(downloaded_files[i].filename) - 1);
        downloaded_files[i].filename[sizeof(downloaded_files[i].filename) - 1] = '\0';
        downloaded_files[i].file_size = fb_host.file_size;

        total_bytes_received += file_bytes_received;
    }

    if (transfer_aborted) {
        free(downloaded_files);
        close(sock_fd);
        return 1;
    }

    /* Step 7: Await MSG_TRANSFER_END */
    rc = recv_frame_header(sock_fd, &hdr);
    if (rc != 0 || hdr.type != MSG_TRANSFER_END) {
        fprintf(stderr, "[CLIENT] Error: Expected MSG_TRANSFER_END boundary, got type 0x%02X (rc=%d)\n",
                (rc == 0 ? hdr.type : 0), rc);
        free(downloaded_files);
        close(sock_fd);
        return 1;
    }

    /* Step 8: Verify Complete Topic Integrity via CRC32 Fingerprint */
    if (ok_host.file_count > 0 && downloaded_files != NULL) {
        qsort(downloaded_files, ok_host.file_count, sizeof(FileEntry), compare_file_entries);
        uint32_t local_fingerprint = compute_manifest_crc32(downloaded_files, ok_host.file_count);

        if (local_fingerprint != ok_host.manifest_fingerprint) {
            fprintf(stderr, "[CLIENT] Integrity Error: Manifest fingerprint mismatch (local 0x%08X != server 0x%08X)\n",
                    local_fingerprint, ok_host.manifest_fingerprint);
            free(downloaded_files);
            close(sock_fd);
            return 1;
        }
    }

    if (total_bytes_received != ok_host.total_topic_bytes) {
        fprintf(stderr, "[CLIENT] Integrity Error: Byte count mismatch (received %lu != expected %lu)\n",
                (unsigned long)total_bytes_received, (unsigned long)ok_host.total_topic_bytes);
        free(downloaded_files);
        close(sock_fd);
        return 1;
    }

    /* Step 9: Report Success and Exit Cleanly */
    printf("[CLIENT] Transfer complete: %u files (%lu bytes) successfully saved to '%s'\n",
           ok_host.file_count, (unsigned long)total_bytes_received, topic_dir);

    free(downloaded_files);
    close(sock_fd);

    return 0;
}
