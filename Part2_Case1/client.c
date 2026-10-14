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
#include <time.h>
#include <ftw.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ----------------------------------------------------------------------------
 * Constants & Configuration
 * ---------------------------------------------------------------------------- */

#define MAX_RETRY_ATTEMPTS   50
#define INITIAL_BACKOFF_USEC 200000   /* 200 ms backoff */
#define MAX_BACKOFF_USEC     2000000  /* 2.0 s maximum backoff */

typedef struct {
    char server_ip[64];
    int server_port;
    char topic[MAX_TOPIC_NAME_LEN];
    char output_dir[FT_MAX_PATH_LEN];
} ClientConfig;

typedef enum {
    TRANSFER_SUCCESS       = 0,
    TRANSFER_NETWORK_ERROR = 1, /* Socket drop, connection reset: retryable */
    TRANSFER_FATAL_ERROR   = 2  /* Topic not found, traversal, invalid argument: non-retryable */
} TransferResult;

/* ----------------------------------------------------------------------------
 * Directory Purge Routine (nftw post-order recursive delete)
 * ---------------------------------------------------------------------------- */

static int remove_entry_callback(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;
    return remove(fpath);
}

static int purge_directory(const char *dir_path) {
    struct stat st;
    if (stat(dir_path, &st) != 0) {
        return 0; /* Path does not exist; nothing to purge */
    }
    if (!S_ISDIR(st.st_mode)) {
        return remove(dir_path);
    }
    return nftw(dir_path, remove_entry_callback, 64, FTW_DEPTH | FTW_PHYS);
}

/* ----------------------------------------------------------------------------
 * CLI Parsing & Validation
 * ---------------------------------------------------------------------------- */

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <server_ip> <server_port> <topic> <output_dir>\n", prog_name);
    fprintf(stderr, "  <server_ip>   : IPv4 address of server (e.g., 127.0.0.1)\n");
    fprintf(stderr, "  <server_port> : TCP listening port (1024-65535)\n");
    fprintf(stderr, "  <topic>       : Topic name to download (e.g., cane)\n");
    fprintf(stderr, "  <output_dir>  : Local directory where topic will be created\n");
}

static bool parse_arguments(int argc, char *argv[], ClientConfig *config) {
    if (argc != 5) {
        print_usage(argv[0]);
        return false;
    }

    /* 1. Server IPv4 Address */
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
    if (strlen(argv[3]) >= MAX_TOPIC_NAME_LEN) {
        fprintf(stderr, "[CLIENT] Error: Topic name '%s' exceeds maximum length (%d bytes)\n",
                argv[3], MAX_TOPIC_NAME_LEN - 1);
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

    return true;
}

/* ----------------------------------------------------------------------------
 * Manifest Verification Helper
 * ---------------------------------------------------------------------------- */

static int compare_file_entries(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    return strcmp(fa->filename, fb->filename);
}

/* ----------------------------------------------------------------------------
 * Single Topic Transfer Attempt Execution
 * ---------------------------------------------------------------------------- */

static TransferResult execute_topic_transfer(const ClientConfig *config,
                                             const char *topic_dir,
                                             uint32_t *out_file_count,
                                             uint64_t *out_total_bytes) {
    /* Step 1: Create TCP socket */
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        fprintf(stderr, "[CLIENT] Socket creation failed: %s\n", strerror(errno));
        return TRANSFER_NETWORK_ERROR;
    }

    /* Step 2: Connect to server */
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)config->server_port);
    inet_pton(AF_INET, config->server_ip, &serv_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "[CLIENT] Failed to connect to server %s:%d: %s\n",
                config->server_ip, config->server_port, strerror(errno));
        close(sock_fd);
        return TRANSFER_NETWORK_ERROR;
    }

    /* Step 3: Transmit MSG_GET Request */
    MsgGetPayload get_req;
    memset(&get_req, 0, sizeof(get_req));
    strncpy(get_req.topic, config->topic, sizeof(get_req.topic) - 1);

    if (send_frame(sock_fd, MSG_GET, FLAG_NONE, &get_req, sizeof(get_req)) != 0) {
        fprintf(stderr, "[CLIENT] Failed to send MSG_GET request: %s\n", strerror(errno));
        close(sock_fd);
        return TRANSFER_NETWORK_ERROR;
    }

    /* Step 4: Receive Server Response (MSG_TOPIC_OK or MSG_TOPIC_ERROR) */
    FrameHeader hdr;
    int rc = recv_frame_header(sock_fd, &hdr);
    if (rc != 0) {
        fprintf(stderr, "[CLIENT] Connection dropped by server before topic response (rc=%d)\n", rc);
        close(sock_fd);
        return TRANSFER_NETWORK_ERROR;
    }

    if (hdr.type == MSG_TOPIC_ERROR) {
        MsgTopicErrorPayload err_wire, err_host;
        if (recv_frame_payload(sock_fd, &hdr, &err_wire, sizeof(err_wire)) != 0) {
            fprintf(stderr, "[CLIENT] Failed to read MSG_TOPIC_ERROR payload\n");
            close(sock_fd);
            return TRANSFER_FATAL_ERROR;
        }
        proto_ntoh_topic_error(&err_host, &err_wire);
        fprintf(stderr, "[CLIENT] Server rejected topic '%s' (Error %u: %s)\n",
                config->topic, err_host.error_code, err_host.error_msg);
        close(sock_fd);
        return TRANSFER_FATAL_ERROR;
    }

    if (hdr.type != MSG_TOPIC_OK) {
        fprintf(stderr, "[CLIENT] Unexpected message type 0x%02X received (expected MSG_TOPIC_OK)\n", hdr.type);
        close(sock_fd);
        return TRANSFER_FATAL_ERROR;
    }

    MsgTopicOkPayload ok_wire, ok_host;
    if (recv_frame_payload(sock_fd, &hdr, &ok_wire, sizeof(ok_wire)) != 0) {
        fprintf(stderr, "[CLIENT] Failed to read MSG_TOPIC_OK payload\n");
        close(sock_fd);
        return TRANSFER_NETWORK_ERROR;
    }
    proto_ntoh_topic_ok(&ok_host, &ok_wire);

    printf("[CLIENT] Server accepted topic '%s': %u files, %lu total bytes (fingerprint 0x%08X)\n",
           config->topic, ok_host.file_count, (unsigned long)ok_host.total_topic_bytes,
           ok_host.manifest_fingerprint);

    /* Step 5: Ensure fresh destination directory exists */
    FileTransferError fterr = create_directory_recursive(topic_dir);
    if (fterr != FT_OK) {
        fprintf(stderr, "[CLIENT] Failed to create directory '%s': %s\n",
                topic_dir, file_transfer_error_to_string(fterr));
        close(sock_fd);
        return TRANSFER_FATAL_ERROR;
    }

    /* Allocate tracking array for downloaded files */
    FileEntry *downloaded_files = NULL;
    if (ok_host.file_count > 0) {
        downloaded_files = calloc(ok_host.file_count, sizeof(FileEntry));
        if (!downloaded_files) {
            fprintf(stderr, "[CLIENT] Memory allocation failed for file tracking array\n");
            close(sock_fd);
            return TRANSFER_FATAL_ERROR;
        }
    }

    uint64_t total_bytes_received = 0;
    bool network_drop_detected = false;
    bool fatal_error_detected = false;

    /* Step 6: Stream files */
    for (uint32_t i = 0; i < ok_host.file_count; i++) {
        /* 6a: Await MSG_FILE_BEGIN */
        rc = recv_frame_header(sock_fd, &hdr);
        if (rc != 0 || hdr.type != MSG_FILE_BEGIN) {
            fprintf(stderr, "[CLIENT] Interrupted awaiting MSG_FILE_BEGIN for file index %u (rc=%d, type=0x%02X)\n",
                    i, rc, (rc == 0 ? hdr.type : 0));
            network_drop_detected = true;
            break;
        }

        MsgFileBeginPayload fb_wire, fb_host;
        if (recv_frame_payload(sock_fd, &hdr, &fb_wire, sizeof(fb_wire)) != 0) {
            fprintf(stderr, "[CLIENT] Connection dropped while reading MSG_FILE_BEGIN payload for file %u\n", i);
            network_drop_detected = true;
            break;
        }
        proto_ntoh_file_begin(&fb_host, &fb_wire);

        if (fb_host.file_index != i) {
            fprintf(stderr, "[CLIENT] Protocol error: File index mismatch (expected %u, received %u)\n",
                    i, fb_host.file_index);
            fatal_error_detected = true;
            break;
        }

        if (validate_filename(fb_host.filename) != FT_OK) {
            fprintf(stderr, "[CLIENT] Security error: Forbidden filename '%s'\n", fb_host.filename);
            fatal_error_detected = true;
            break;
        }

        /* 6b: Open destination file */
        char dest_path[2048];
        fterr = build_destination_path(config->output_dir, config->topic, fb_host.filename, dest_path, sizeof(dest_path));
        if (fterr != FT_OK) {
            fprintf(stderr, "[CLIENT] Failed to build destination path for '%s': %s\n",
                    fb_host.filename, file_transfer_error_to_string(fterr));
            fatal_error_detected = true;
            break;
        }

        int file_fd = open_file_for_writing(dest_path, true);
        if (file_fd < 0) {
            fprintf(stderr, "[CLIENT] Failed to open '%s' for writing: %s\n", dest_path, strerror(errno));
            fatal_error_detected = true;
            break;
        }

        /* 6c: Receive MSG_FILE_DATA chunks */
        uint64_t file_bytes_received = 0;
        char chunk_buf[FT_CHUNK_SIZE];

        while (file_bytes_received < fb_host.file_size) {
            rc = recv_frame_header(sock_fd, &hdr);
            if (rc != 0 || hdr.type != MSG_FILE_DATA) {
                fprintf(stderr, "[CLIENT] Server connection severed during data transfer of '%s' (offset %lu/%lu)\n",
                        fb_host.filename, (unsigned long)file_bytes_received, (unsigned long)fb_host.file_size);
                network_drop_detected = true;
                break;
            }

            if (hdr.payload_len == 0 || hdr.payload_len > FT_CHUNK_SIZE) {
                fprintf(stderr, "[CLIENT] Invalid chunk payload length %lu bytes\n", (unsigned long)hdr.payload_len);
                fatal_error_detected = true;
                break;
            }

            if (file_bytes_received + hdr.payload_len > fb_host.file_size) {
                fprintf(stderr, "[CLIENT] Chunk exceeds announced file size\n");
                fatal_error_detected = true;
                break;
            }

            if (recv_frame_payload(sock_fd, &hdr, chunk_buf, sizeof(chunk_buf)) != 0) {
                fprintf(stderr, "[CLIENT] Connection dropped while reading chunk payload for '%s'\n", fb_host.filename);
                network_drop_detected = true;
                break;
            }

            FileTransferError werr = write_chunk_to_file(file_fd, chunk_buf, (size_t)hdr.payload_len);
            if (werr != FT_OK) {
                fprintf(stderr, "[CLIENT] Disk write error for '%s': %s\n",
                        fb_host.filename, file_transfer_error_to_string(werr));
                fatal_error_detected = true;
                break;
            }

            file_bytes_received += hdr.payload_len;
        }

        if (network_drop_detected || fatal_error_detected) {
            close_file(&file_fd);
            break;
        }

        /* 6d: Await MSG_FILE_END */
        rc = recv_frame_header(sock_fd, &hdr);
        if (rc != 0 || hdr.type != MSG_FILE_END) {
            fprintf(stderr, "[CLIENT] Interrupted awaiting MSG_FILE_END for '%s' (rc=%d)\n", fb_host.filename, rc);
            close_file(&file_fd);
            network_drop_detected = true;
            break;
        }

        MsgFileEndPayload fe_wire, fe_host;
        if (recv_frame_payload(sock_fd, &hdr, &fe_wire, sizeof(fe_wire)) != 0) {
            fprintf(stderr, "[CLIENT] Connection dropped while reading MSG_FILE_END payload for '%s'\n", fb_host.filename);
            close_file(&file_fd);
            network_drop_detected = true;
            break;
        }
        proto_ntoh_file_end(&fe_host, &fe_wire);

        if (fe_host.file_index != i || fe_host.final_offset != fb_host.file_size) {
            fprintf(stderr, "[CLIENT] File end offset mismatch for '%s'\n", fb_host.filename);
            close_file(&file_fd);
            fatal_error_detected = true;
            break;
        }

        sync_file_to_disk(file_fd);
        close_file(&file_fd);

        strncpy(downloaded_files[i].filename, fb_host.filename, sizeof(downloaded_files[i].filename) - 1);
        downloaded_files[i].filename[sizeof(downloaded_files[i].filename) - 1] = '\0';
        downloaded_files[i].file_size = fb_host.file_size;

        total_bytes_received += file_bytes_received;
    }

    if (network_drop_detected) {
        free(downloaded_files);
        close(sock_fd);
        return TRANSFER_NETWORK_ERROR;
    }

    if (fatal_error_detected) {
        free(downloaded_files);
        close(sock_fd);
        return TRANSFER_FATAL_ERROR;
    }

    /* Step 7: Await MSG_TRANSFER_END */
    rc = recv_frame_header(sock_fd, &hdr);
    if (rc != 0 || hdr.type != MSG_TRANSFER_END) {
        fprintf(stderr, "[CLIENT] Interrupted awaiting MSG_TRANSFER_END boundary (rc=%d)\n", rc);
        free(downloaded_files);
        close(sock_fd);
        return TRANSFER_NETWORK_ERROR;
    }

    /* Step 8: Verify Topic Integrity via CRC32 Fingerprint */
    if (ok_host.file_count > 0 && downloaded_files != NULL) {
        qsort(downloaded_files, ok_host.file_count, sizeof(FileEntry), compare_file_entries);
        uint32_t local_fingerprint = compute_manifest_crc32(downloaded_files, ok_host.file_count);

        if (local_fingerprint != ok_host.manifest_fingerprint) {
            fprintf(stderr, "[CLIENT] Integrity failure: CRC32 fingerprint mismatch (local 0x%08X != server 0x%08X)\n",
                    local_fingerprint, ok_host.manifest_fingerprint);
            free(downloaded_files);
            close(sock_fd);
            return TRANSFER_FATAL_ERROR;
        }
    }

    if (total_bytes_received != ok_host.total_topic_bytes) {
        fprintf(stderr, "[CLIENT] Integrity failure: Total byte count mismatch (received %lu != expected %lu)\n",
                (unsigned long)total_bytes_received, (unsigned long)ok_host.total_topic_bytes);
        free(downloaded_files);
        close(sock_fd);
        return TRANSFER_FATAL_ERROR;
    }

    *out_file_count = ok_host.file_count;
    *out_total_bytes = total_bytes_received;

    free(downloaded_files);
    close(sock_fd);
    return TRANSFER_SUCCESS;
}

/* ----------------------------------------------------------------------------
 * Main Client Flow with Retry Loop & Directory Purge
 * ---------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
    ClientConfig config;
    if (!parse_arguments(argc, argv, &config)) {
        return 1;
    }

    char topic_dir[2048];
    snprintf(topic_dir, sizeof(topic_dir), "%s/%s", config.output_dir, config.topic);

    printf("===============================================================\n");
    printf("IT305 Fault-Tolerant Topic File Client (Part 2 Case 1)\n");
    printf("Server Target    : %s:%d\n", config.server_ip, config.server_port);
    printf("Topic Requested  : %s\n", config.topic);
    printf("Output Directory : %s\n", topic_dir);
    printf("Recovery Policy  : Purge Partial Data & Full Restart\n");
    printf("Max Retries      : %d\n", MAX_RETRY_ATTEMPTS);
    printf("===============================================================\n");

    int attempt = 0;
    bool success = false;
    useconds_t backoff_usec = INITIAL_BACKOFF_USEC;
    uint32_t final_file_count = 0;
    uint64_t final_total_bytes = 0;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (attempt <= MAX_RETRY_ATTEMPTS) {
        attempt++;
        printf("\n[CLIENT] ---> Transfer Attempt #%d for topic '%s'...\n", attempt, config.topic);

        /* Ensure destination directory is purged of any partial residue before attempt */
        purge_directory(topic_dir);

        TransferResult res = execute_topic_transfer(&config, topic_dir, &final_file_count, &final_total_bytes);

        if (res == TRANSFER_SUCCESS) {
            success = true;
            break;
        }

        if (res == TRANSFER_FATAL_ERROR) {
            fprintf(stderr, "[CLIENT] Fatal unrecoverable error encountered. Aborting transfer.\n");
            purge_directory(topic_dir);
            return 1;
        }

        /* TRANSFER_NETWORK_ERROR: Disconnect detected; purge and retry */
        fprintf(stderr, "[CLIENT] Transfer failed on attempt %d due to connection termination.\n", attempt);
        printf("[CLIENT] Purging incomplete topic directory '%s' to eliminate residue...\n", topic_dir);
        purge_directory(topic_dir);

        if (attempt > MAX_RETRY_ATTEMPTS) {
            fprintf(stderr, "[CLIENT] Error: Maximum retry limit (%d) reached without completing transfer.\n",
                    MAX_RETRY_ATTEMPTS);
            break;
        }

        printf("[CLIENT] Backing off for %u ms before reconnecting...\n",
               (unsigned int)(backoff_usec / 1000));
        usleep(backoff_usec);

        /* Exponential backoff */
        backoff_usec = (backoff_usec * 2 > MAX_BACKOFF_USEC) ? MAX_BACKOFF_USEC : (backoff_usec * 2);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_sec = (double)(end_time.tv_sec - start_time.tv_sec) +
                         (double)(end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    if (!success) {
        fprintf(stderr, "\n[CLIENT] FAILED: Topic '%s' could not be transferred after %d attempts (%.2f seconds).\n",
                config.topic, attempt - 1, elapsed_sec);
        purge_directory(topic_dir);
        return 1;
    }

    printf("\n===============================================================\n");
    printf("[CLIENT] SUCCESS: Topic '%s' successfully transferred!\n", config.topic);
    printf("[CLIENT] Total Files Saved   : %u\n", final_file_count);
    printf("[CLIENT] Total Bytes Saved   : %lu bytes\n", (unsigned long)final_total_bytes);
    printf("[CLIENT] Total Attempts      : %d (Retries: %d)\n", attempt, attempt - 1);
    printf("[CLIENT] Elapsed Total Time  : %.3f seconds\n", elapsed_sec);
    printf("[CLIENT] Saved Directory     : %s\n", topic_dir);
    printf("===============================================================\n");

    return 0;
}
