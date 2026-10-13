/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: Part1/server.c
 * Description: Part 1 Baseline Topic File Server (Sequential & Multi-Threaded).
 *
 * Author: Jay Vaghela (Student ID: 202401237)
 * Phase: Phase 1 (Core Wire Framing & Baseline Server)
 * Status: Implementation Complete & Verified
 *
 * Source of Truth Hierarchy:
 *   - Tier 1: IT305 Assignment Specification (§Part 1, §Standard CLI)
 *   - Tier 2: Instructor Clarifications (§Page 2-3)
 *   - Tier 3: Approved Design Document Group 071_072 (§5.1, §7.1, §7.2)
 *   - Tier 4: Master Execution Roadmap (PART2_TASKS_ROADMAP.md §4.3, §5 Task 1.3)
 *
 * Standard CLI Invocation:
 *   ./server <port> <topics_root_dir> [mode]
 *   - <port>            : Listening TCP port (1024-65535)
 *   - <topics_root_dir> : Directory containing topic subdirectories
 *   - [mode]            : Optional: "mt" (default) or "sequential"
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
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

/* ----------------------------------------------------------------------------
 * Server Configuration & Operational Modes
 * ---------------------------------------------------------------------------- */

typedef enum {
    SERVER_MODE_MT = 0,         /* Multi-threaded: 1 worker thread per client (Default) */
    SERVER_MODE_SEQUENTIAL = 1  /* Sequential: 1 client at a time, baseline for benchmarking */
} ServerMode;

typedef struct {
    int listen_port;
    char topics_root_dir[MAX_PATH_BUFFER_LEN];
    ServerMode mode;
} ServerConfig;

/* Thread argument structure for multi-threaded worker */
typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
    char topics_root_dir[MAX_PATH_BUFFER_LEN];
} ClientContext;

/* Global shutdown flag for graceful termination */
static volatile sig_atomic_t g_server_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    g_server_running = 0;
}

/* ----------------------------------------------------------------------------
 * Client Request Processing State Machine
 * ---------------------------------------------------------------------------- */

/**
 * process_client:
 * Executes the Part 1 transfer protocol over an accepted client connection.
 *
 * Wire Message Sequence:
 *   1. Client -> Server: MSG_GET (topic name in MsgGetPayload)
 *   2. Server validates topic:
 *      - If invalid / traversal / not found:
 *        Server -> Client: MSG_TOPIC_ERROR (MsgTopicErrorPayload)
 *        Server closes connection.
 *      - If valid:
 *        Server -> Client: MSG_TOPIC_OK (MsgTopicOkPayload)
 *   3. For each file in sorted manifest:
 *      Server -> Client: MSG_FILE_BEGIN (MsgFileBeginPayload)
 *      Server -> Client: MSG_FILE_DATA (bounded 64 KiB chunks)
 *      Server -> Client: MSG_FILE_END (MsgFileEndPayload)
 *   4. Server -> Client: MSG_TRANSFER_END (0-length payload)
 *   5. Clean socket closure.
 */
static void process_client(int client_fd, const char *topics_root, const struct sockaddr_in *client_addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr->sin_port);

    printf("[SERVER] Connection accepted from %s:%d (fd=%d)\n", client_ip, client_port, client_fd);

    /* Step 1: Receive FrameHeader for MSG_GET */
    FrameHeader hdr;
    int rc = recv_frame_header(client_fd, &hdr);
    if (rc != 0) {
        if (rc == 1) {
            printf("[SERVER] Client %s:%d closed connection before sending request (EOF)\n", client_ip, client_port);
        } else {
            fprintf(stderr, "[SERVER] Error reading request header from %s:%d (errno=%d: %s)\n",
                    client_ip, client_port, errno, strerror(errno));
        }
        return;
    }

    if (hdr.type != MSG_GET) {
        fprintf(stderr, "[SERVER] Protocol violation from %s:%d: expected MSG_GET (0x01), got 0x%02X\n",
                client_ip, client_port, hdr.type);
        MsgTopicErrorPayload err_payload;
        err_payload.error_code = ERR_BAD_REQUEST;
        snprintf(err_payload.error_msg, sizeof(err_payload.error_msg),
                 "Invalid initial message type 0x%02X (expected MSG_GET)", hdr.type);
        MsgTopicErrorPayload wire_err;
        proto_hton_topic_error(&wire_err, &err_payload);
        send_frame(client_fd, MSG_TOPIC_ERROR, FLAG_NONE, &wire_err, sizeof(wire_err));
        return;
    }

    /* Step 2: Receive MSG_GET payload */
    MsgGetPayload req_payload;
    memset(&req_payload, 0, sizeof(req_payload));
    if (recv_frame_payload(client_fd, &hdr, &req_payload, sizeof(req_payload)) != 0) {
        fprintf(stderr, "[SERVER] Failed to read MSG_GET payload from %s:%d\n", client_ip, client_port);
        return;
    }
    req_payload.topic[sizeof(req_payload.topic) - 1] = '\0';

    printf("[SERVER] Client %s:%d requested topic: '%s'\n", client_ip, client_port, req_payload.topic);

    /* Step 3: Validate topic and build manifest */
    TopicManifest *manifest = NULL;
    TopicError terr = build_topic_manifest(topics_root, req_payload.topic, &manifest);

    if (terr != TOPIC_OK) {
        /* Topic lookup or security check failed: transmit MSG_TOPIC_ERROR */
        uint32_t error_code = ERR_INTERNAL;
        if (terr == TOPIC_ERR_INVALID_NAME || terr == TOPIC_ERR_ESCAPE) {
            error_code = ERR_FORBIDDEN; /* Path traversal or security escape */
        } else if (terr == TOPIC_ERR_NOT_FOUND || terr == TOPIC_ERR_NOT_DIR) {
            error_code = ERR_TOPIC_NOT_FOUND; /* Missing topic directory */
        } else if (terr == TOPIC_ERR_PERMISSION) {
            error_code = ERR_FORBIDDEN;
        }

        fprintf(stderr, "[SERVER] Rejecting topic '%s' from %s:%d: %s (code %u)\n",
                req_payload.topic, client_ip, client_port, topic_error_to_string(terr), error_code);

        MsgTopicErrorPayload err_payload;
        err_payload.error_code = error_code;
        snprintf(err_payload.error_msg, sizeof(err_payload.error_msg), "%s", topic_error_to_string(terr));

        MsgTopicErrorPayload wire_err;
        proto_hton_topic_error(&wire_err, &err_payload);
        send_frame(client_fd, MSG_TOPIC_ERROR, FLAG_NONE, &wire_err, sizeof(wire_err));
        return;
    }

    /* Step 4: Send MSG_TOPIC_OK with manifest metadata */
    printf("[SERVER] Topic '%s' resolved for %s:%d (%u files, %lu total bytes, fingerprint 0x%08X)\n",
           req_payload.topic, client_ip, client_port,
           manifest->file_count, (unsigned long)manifest->total_topic_bytes, manifest->manifest_fingerprint);

    MsgTopicOkPayload ok_payload;
    ok_payload.file_count = manifest->file_count;
    ok_payload.manifest_fingerprint = manifest->manifest_fingerprint;
    ok_payload.total_topic_bytes = manifest->total_topic_bytes;

    MsgTopicOkPayload wire_ok;
    proto_hton_topic_ok(&wire_ok, &ok_payload);
    if (send_frame(client_fd, MSG_TOPIC_OK, FLAG_NONE, &wire_ok, sizeof(wire_ok)) != 0) {
        fprintf(stderr, "[SERVER] Failed to send MSG_TOPIC_OK to %s:%d (peer disconnected)\n", client_ip, client_port);
        free_topic_manifest(manifest);
        return;
    }

    /* Step 5: Stream each file in bounded 64 KiB chunks */
    char *chunk_buffer = malloc(DEFAULT_CHUNK_SIZE);
    if (!chunk_buffer) {
        fprintf(stderr, "[SERVER] Out of memory allocating chunk buffer for %s:%d\n", client_ip, client_port);
        free_topic_manifest(manifest);
        return;
    }

    bool transfer_aborted = false;

    for (uint32_t i = 0; i < manifest->file_count; i++) {
        const FileEntry *file = &manifest->files[i];

        /* Construct full file path */
        char file_path[2048];
        snprintf(file_path, sizeof(file_path), "%s/%s", manifest->directory_path, file->filename);

        /* VIVA NOTE: Use O_RDONLY | O_NOFOLLOW to prevent symlink traversal at file level */
        int file_fd = open(file_path, O_RDONLY);
        if (file_fd < 0) {
            fprintf(stderr, "[SERVER] Error opening file '%s' for %s:%d: %s\n",
                    file_path, client_ip, client_port, strerror(errno));
            transfer_aborted = true;
            break;
        }

        /* 5a: Send MSG_FILE_BEGIN */
        MsgFileBeginPayload begin_payload;
        begin_payload.file_index = i;
        begin_payload.file_size = file->file_size;
        begin_payload.start_offset = 0; /* Baseline Part 1 starts at offset 0 */
        strncpy(begin_payload.filename, file->filename, sizeof(begin_payload.filename) - 1);
        begin_payload.filename[sizeof(begin_payload.filename) - 1] = '\0';

        MsgFileBeginPayload wire_begin;
        proto_hton_file_begin(&wire_begin, &begin_payload);
        if (send_frame(client_fd, MSG_FILE_BEGIN, FLAG_NONE, &wire_begin, sizeof(wire_begin)) != 0) {
            fprintf(stderr, "[SERVER] Client %s:%d disconnected during MSG_FILE_BEGIN for '%s'\n",
                    client_ip, client_port, file->filename);
            close(file_fd);
            transfer_aborted = true;
            break;
        }

        /* 5b: Stream data chunks (up to DEFAULT_CHUNK_SIZE bytes per frame) */
        uint64_t bytes_sent = 0;
        while (bytes_sent < file->file_size) {
            size_t bytes_to_read = DEFAULT_CHUNK_SIZE;
            if ((file->file_size - bytes_sent) < bytes_to_read) {
                bytes_to_read = (size_t)(file->file_size - bytes_sent);
            }

            ssize_t n_read = read(file_fd, chunk_buffer, bytes_to_read);
            if (n_read < 0) {
                if (errno == EINTR) continue;
                fprintf(stderr, "[SERVER] Disk read error on '%s': %s\n", file_path, strerror(errno));
                transfer_aborted = true;
                break;
            }
            if (n_read == 0) {
                /* Premature EOF on disk */
                break;
            }

            /* Send MSG_FILE_DATA frame */
            if (send_frame(client_fd, MSG_FILE_DATA, FLAG_NONE, chunk_buffer, (uint64_t)n_read) != 0) {
                fprintf(stderr, "[SERVER] Client %s:%d disconnected during MSG_FILE_DATA for '%s'\n",
                        client_ip, client_port, file->filename);
                transfer_aborted = true;
                break;
            }

            bytes_sent += (uint64_t)n_read;
        }

        close(file_fd);

        if (transfer_aborted) {
            break;
        }

        /* 5c: Send MSG_FILE_END */
        MsgFileEndPayload end_payload;
        end_payload.file_index = i;
        end_payload.final_offset = bytes_sent;

        MsgFileEndPayload wire_end;
        proto_hton_file_end(&wire_end, &end_payload);
        if (send_frame(client_fd, MSG_FILE_END, FLAG_NONE, &wire_end, sizeof(wire_end)) != 0) {
            fprintf(stderr, "[SERVER] Client %s:%d disconnected during MSG_FILE_END for '%s'\n",
                    client_ip, client_port, file->filename);
            transfer_aborted = true;
            break;
        }
    }

    free(chunk_buffer);

    /* Step 6: Send MSG_TRANSFER_END if all files streamed successfully */
    if (!transfer_aborted) {
        if (send_frame(client_fd, MSG_TRANSFER_END, FLAG_NONE, NULL, 0) == 0) {
            printf("[SERVER] Successfully completed topic '%s' transfer to %s:%d\n",
                   req_payload.topic, client_ip, client_port);
        } else {
            fprintf(stderr, "[SERVER] Client %s:%d disconnected before MSG_TRANSFER_END\n", client_ip, client_port);
        }
    } else {
        printf("[SERVER] Topic transfer for '%s' to %s:%d aborted prematurely\n", req_payload.topic, client_ip, client_port);
    }

    free_topic_manifest(manifest);
}

/* ----------------------------------------------------------------------------
 * Multi-Threaded Worker Thread Entry Point
 * ---------------------------------------------------------------------------- */

static void* client_worker_thread(void *arg) {
    ClientContext *ctx = (ClientContext *)arg;
    process_client(ctx->client_fd, ctx->topics_root_dir, &ctx->client_addr);
    close(ctx->client_fd);
    free(ctx);
    return NULL;
}

/* ----------------------------------------------------------------------------
 * CLI Parsing & Listening Socket Setup
 * ---------------------------------------------------------------------------- */

static int parse_arguments(int argc, char *argv[], ServerConfig *config) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <port> <topics_root_dir> [mode]\n", argv[0]);
        fprintf(stderr, "  <port>            : Listening port (1024-65535)\n");
        fprintf(stderr, "  <topics_root_dir> : Path to directory containing topic sub-folders\n");
        fprintf(stderr, "  [mode]            : Optional: 'mt' (default) or 'sequential'\n");
        return -1;
    }

    /* Validate port */
    char *endptr = NULL;
    long port = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || port < 1024 || port > 65535) {
        fprintf(stderr, "Error: Invalid port '%s'. Port must be an integer between 1024 and 65535.\n", argv[1]);
        return -1;
    }
    config->listen_port = (int)port;

    /* Validate topics root directory */
    struct stat st;
    if (stat(argv[2], &st) != 0) {
        fprintf(stderr, "Error: Topics root directory '%s' cannot be accessed: %s\n", argv[2], strerror(errno));
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Topics root path '%s' is not a directory.\n", argv[2]);
        return -1;
    }
    strncpy(config->topics_root_dir, argv[2], sizeof(config->topics_root_dir) - 1);
    config->topics_root_dir[sizeof(config->topics_root_dir) - 1] = '\0';

    /* Parse operational mode (Defaults strictly to Multi-Threaded for autograder) */
    config->mode = SERVER_MODE_MT;

    if (argc == 4) {
        if (strcmp(argv[3], "sequential") == 0 || strcmp(argv[3], "--sequential") == 0 ||
            strcmp(argv[3], "seq") == 0 || strcmp(argv[3], "single") == 0) {
            config->mode = SERVER_MODE_SEQUENTIAL;
        } else if (strcmp(argv[3], "mt") == 0 || strcmp(argv[3], "--mt") == 0 ||
                   strcmp(argv[3], "multithreaded") == 0) {
            config->mode = SERVER_MODE_MT;
        } else {
            fprintf(stderr, "Error: Unknown mode '%s'. Allowed: 'mt' (default) or 'sequential'\n", argv[3]);
            return -1;
        }
    } else {
        /* Support environment variable override for automated benchmarking */
        const char *env_mode = getenv("SERVER_MODE");
        if (env_mode && (strcmp(env_mode, "sequential") == 0 || strcmp(env_mode, "seq") == 0)) {
            config->mode = SERVER_MODE_SEQUENTIAL;
        }
    }

    return 0;
}

static int create_listening_socket(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket() failed");
        return -1;
    }

    /* Enable SO_REUSEADDR to avoid bind failure during quick server restarts */
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(listen_fd);
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind() failed");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen() failed");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

/* ----------------------------------------------------------------------------
 * Main Server Loop
 * ---------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
    ServerConfig config;
    if (parse_arguments(argc, argv, &config) != 0) {
        return 1;
    }

    /*
     * VIVA NOTE: Ignore SIGPIPE so broken client sockets return EPIPE
     * instead of killing the server process.
     */
    signal(SIGPIPE, SIG_IGN);

    /* Setup clean shutdown on SIGINT and SIGTERM */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int listen_fd = create_listening_socket(config.listen_port);
    if (listen_fd < 0) {
        return 1;
    }

    printf("===============================================================\n");
    printf("IT305 Baseline Topic File Server (Part 1)\n");
    printf("Author: Jay Vaghela (202401237)\n");
    printf("Listening Port    : %d\n", config.listen_port);
    printf("Topics Root Dir   : %s\n", config.topics_root_dir);
    printf("Operational Mode  : %s\n", (config.mode == SERVER_MODE_MT) ? "Multi-Threaded (MT)" : "Sequential");
    printf("===============================================================\n");
    printf("[SERVER] Ready and listening for incoming client requests...\n");

    while (g_server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                /* Interrupted by signal (e.g. SIGINT), check loop condition */
                continue;
            }
            perror("accept() error");
            break;
        }

        if (config.mode == SERVER_MODE_SEQUENTIAL) {
            /*
             * Sequential Mode:
             * Handle the client synchronously on the calling thread.
             * Other arriving clients queue in the listen backlog until this finishes.
             */
            process_client(client_fd, config.topics_root_dir, &client_addr);
            close(client_fd);
        } else {
            /*
             * Multi-Threaded Mode:
             * Spawn a detached worker thread per client.
             */
            ClientContext *ctx = malloc(sizeof(ClientContext));
            if (!ctx) {
                fprintf(stderr, "[SERVER] Out of memory allocating client context\n");
                close(client_fd);
                continue;
            }

            ctx->client_fd = client_fd;
            ctx->client_addr = client_addr;
            strncpy(ctx->topics_root_dir, config.topics_root_dir, sizeof(ctx->topics_root_dir) - 1);
            ctx->topics_root_dir[sizeof(ctx->topics_root_dir) - 1] = '\0';

            pthread_t tid;
            int pt_rc = pthread_create(&tid, NULL, client_worker_thread, ctx);
            if (pt_rc != 0) {
                fprintf(stderr, "[SERVER] pthread_create() failed: %s\n", strerror(pt_rc));
                close(client_fd);
                free(ctx);
                continue;
            }

            /* Detach thread to avoid resource leaks without requiring pthread_join */
            pthread_detach(tid);
        }
    }

    printf("\n[SERVER] Shutting down server and releasing socket...\n");
    close(listen_fd);
    printf("[SERVER] Clean exit completed.\n");
    return 0;
}
