/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: Part2_Case2/server.c
 * Description: Simplified Topic File Server with In-Memory Session Resume.
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "protocol.h"
#include "topic_manager.h"
#include "fault_injection.h"
#include "session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

typedef struct {
    int client_fd;
    char topics_root_dir[1024];
} ClientContext;

/* Helper to send error frames */
static void send_topic_error(int fd, uint32_t code, const char *msg) {
    MsgTopicErrorPayload err = { code, "" };
    if (msg) {
        strncpy(err.error_msg, msg, sizeof(err.error_msg) - 1);
    }
    MsgTopicErrorPayload wire_err;
    proto_hton_topic_error(&wire_err, &err);
    send_frame(fd, MSG_TOPIC_ERROR, FLAG_NONE, &wire_err, sizeof(wire_err));
}

static void send_resume_error(int fd, uint32_t code, const char *msg) {
    MsgTopicErrorPayload err = { code, "" };
    if (msg) {
        strncpy(err.error_msg, msg, sizeof(err.error_msg) - 1);
    }
    MsgTopicErrorPayload wire_err;
    proto_hton_topic_error(&wire_err, &err);
    send_frame(fd, MSG_RESUME_ERROR, FLAG_NONE, &wire_err, sizeof(wire_err));
}

/* Worker thread handling one client connection */
static void* client_worker(void *arg) {
    ClientContext *ctx = (ClientContext *)arg;
    int client_fd = ctx->client_fd;
    const char *topics_root = ctx->topics_root_dir;

    FrameHeader hdr;
    if (recv_frame_header(client_fd, &hdr) != 0) {
        close(client_fd);
        free(ctx);
        return NULL;
    }

    uint64_t session_id = 0;
    char topic[MAX_TOPIC_NAME_LEN];
    memset(topic, 0, sizeof(topic));
    TopicManifest *manifest = NULL;
    uint32_t start_file_idx = 0;
    uint64_t start_file_offset = 0;

    if (hdr.type == MSG_SESSION_START) {
        MsgSessionStartPayload req;
        if (recv_frame_payload(client_fd, &hdr, &req, sizeof(req)) != 0) {
            close(client_fd);
            free(ctx);
            return NULL;
        }
        req.topic[sizeof(req.topic) - 1] = '\0';
        strncpy(topic, req.topic, sizeof(topic) - 1);

        TopicError terr = build_topic_manifest(topics_root, topic, &manifest);
        if (terr != TOPIC_OK) {
            send_topic_error(client_fd, (terr == TOPIC_ERR_NOT_FOUND) ? 404 : 403, topic_error_to_string(terr));
            close(client_fd);
            free(ctx);
            return NULL;
        }

        if (session_create(topic, manifest->manifest_fingerprint, manifest->file_count, &session_id) != 0) {
            send_topic_error(client_fd, 500, "Session table full");
            free_topic_manifest(manifest);
            close(client_fd);
            free(ctx);
            return NULL;
        }

        MsgSessionAcceptPayload accept_payload = { session_id, manifest->manifest_fingerprint, manifest->file_count };
        MsgSessionAcceptPayload wire_accept;
        proto_hton_session_accept(&wire_accept, &accept_payload);
        if (send_frame(client_fd, MSG_SESSION_ACCEPT, FLAG_NONE, &wire_accept, sizeof(wire_accept)) != 0) {
            free_topic_manifest(manifest);
            close(client_fd);
            free(ctx);
            return NULL;
        }

        start_file_idx = 0;
        start_file_offset = 0;

    } else if (hdr.type == MSG_SESSION_RESUME) {
        MsgSessionResumePayload wire_resume, req;
        if (recv_frame_payload(client_fd, &hdr, &wire_resume, sizeof(wire_resume)) != 0) {
            close(client_fd);
            free(ctx);
            return NULL;
        }
        proto_ntoh_session_resume(&req, &wire_resume);
        req.topic[sizeof(req.topic) - 1] = '\0';
        strncpy(topic, req.topic, sizeof(topic) - 1);
        session_id = req.session_id;

        TopicError terr = build_topic_manifest(topics_root, topic, &manifest);
        if (terr != TOPIC_OK) {
            send_resume_error(client_fd, 404, "Topic directory inaccessible");
            close(client_fd);
            free(ctx);
            return NULL;
        }

        uint32_t auth_file = 0;
        uint64_t auth_offset = 0;
        uint16_t flags = 0;
        int vres = session_validate_resume(session_id, topic, manifest->manifest_fingerprint,
                                           req.proposed_file_index, req.proposed_file_offset,
                                           &auth_file, &auth_offset, &flags);
        if (vres != 0) {
            if (vres == -6) {
                send_resume_error(client_fd, 409, "MANIFEST_CHANGED: Fingerprint mismatch");
            } else if (vres == -2) {
                send_resume_error(client_fd, 404, "SESSION_NOT_FOUND: Unknown session ID");
            } else if (vres == -3) {
                send_resume_error(client_fd, 410, "SESSION_EXPIRED: TTL exceeded");
            } else {
                send_resume_error(client_fd, 400, "RESUME_REJECTED: Invalid resume state");
            }
            free_topic_manifest(manifest);
            close(client_fd);
            free(ctx);
            return NULL;
        }

        MsgResumeAcceptPayload accept_payload = { session_id, auth_file, auth_offset, flags };
        MsgResumeAcceptPayload wire_accept;
        proto_hton_resume_accept(&wire_accept, &accept_payload);
        if (send_frame(client_fd, MSG_RESUME_ACCEPT, flags, &wire_accept, sizeof(wire_accept)) != 0) {
            free_topic_manifest(manifest);
            close(client_fd);
            free(ctx);
            return NULL;
        }

        start_file_idx = auth_file;
        start_file_offset = auth_offset;

    } else {
        send_topic_error(client_fd, 400, "Expected MSG_SESSION_START or MSG_SESSION_RESUME");
        close(client_fd);
        free(ctx);
        return NULL;
    }

    /* Stream files starting from authoritative checkpoint */
    char chunk[DEFAULT_CHUNK_SIZE];
    bool stream_ok = true;

    for (uint32_t i = start_file_idx; i < manifest->file_count; i++) {
        uint64_t file_start = (i == start_file_idx) ? start_file_offset : 0;
        char file_path[2048];
        snprintf(file_path, sizeof(file_path), "%s/%s", manifest->directory_path, manifest->files[i].filename);

        int file_fd = open(file_path, O_RDONLY);
        if (file_fd < 0) {
            stream_ok = false;
            break;
        }

        if (file_start > 0) {
            if (lseek(file_fd, (off_t)file_start, SEEK_SET) == (off_t)-1) {
                close(file_fd);
                stream_ok = false;
                break;
            }
        }

        MsgFileBeginPayload begin = { i, manifest->files[i].file_size, file_start, "" };
        strncpy(begin.filename, manifest->files[i].filename, sizeof(begin.filename) - 1);
        MsgFileBeginPayload wire_begin;
        proto_hton_file_begin(&wire_begin, &begin);
        if (send_frame(client_fd, MSG_FILE_BEGIN, FLAG_NONE, &wire_begin, sizeof(wire_begin)) != 0) {
            close(file_fd);
            stream_ok = false;
            break;
        }

        uint64_t bytes_sent = file_start;

        while (bytes_sent < manifest->files[i].file_size) {
            size_t to_read = DEFAULT_CHUNK_SIZE;
            if (manifest->files[i].file_size - bytes_sent < to_read) {
                to_read = (size_t)(manifest->files[i].file_size - bytes_sent);
            }

            ssize_t n = read(file_fd, chunk, to_read);
            if (n <= 0) {
                break;
            }

            /* Probabilistic Fault Injection Point */
            if (should_inject_fault()) {
                char desc[1024];
                snprintf(desc, sizeof(desc), "Session 0x%016lX: File '%s' offset %lu/%lu",
                         (unsigned long)session_id, manifest->files[i].filename,
                         (unsigned long)bytes_sent, (unsigned long)manifest->files[i].file_size);
                execute_fault_disconnect(client_fd, desc);
                close(file_fd);
                free_topic_manifest(manifest);
                free(ctx);
                return NULL; /* Abruptly dropped; session preserved for resume */
            }

            if (send_frame(client_fd, MSG_FILE_DATA, FLAG_NONE, chunk, (uint64_t)n) != 0) {
                stream_ok = false;
                break;
            }

            /* Await synchronous MSG_CHUNK_ACK before committing */
            FrameHeader ack_hdr;
            if (recv_frame_header(client_fd, &ack_hdr) != 0 || ack_hdr.type != MSG_CHUNK_ACK) {
                stream_ok = false;
                break;
            }
            MsgChunkAckPayload wire_ack, ack;
            if (recv_frame_payload(client_fd, &ack_hdr, &wire_ack, sizeof(wire_ack)) != 0) {
                stream_ok = false;
                break;
            }
            proto_ntoh_chunk_ack(&ack, &wire_ack);

            if (session_commit_chunk(session_id, i, ack.end_offset, manifest->files[i].file_size) < 0) {
                stream_ok = false;
                break;
            }

            bytes_sent = ack.end_offset;
        }

        close(file_fd);

        if (!stream_ok || bytes_sent < manifest->files[i].file_size) {
            stream_ok = false;
            break;
        }

        /* Send MSG_FILE_END and advance checkpoint to next file */
        MsgFileEndPayload end = { i, bytes_sent };
        MsgFileEndPayload wire_end;
        proto_hton_file_end(&wire_end, &end);
        if (send_frame(client_fd, MSG_FILE_END, FLAG_NONE, &wire_end, sizeof(wire_end)) != 0) {
            stream_ok = false;
            break;
        }

        session_complete_file(session_id, i);
    }

    if (stream_ok) {
        if (send_frame(client_fd, MSG_TRANSFER_END, FLAG_NONE, NULL, 0) == 0) {
            session_mark_complete(session_id);
        }
    }

    free_topic_manifest(manifest);
    close(client_fd);
    free(ctx);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <topics_root_dir> <failure probability>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port < 1024 || port > 65535) {
        fprintf(stderr, "Error: Port must be between 1024 and 65535\n");
        return 1;
    }

    struct stat st;
    if (stat(argv[2], &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Topics root directory '%s' does not exist\n", argv[2]);
        return 1;
    }

    double failure_prob = 0.0;
    if (parse_failure_probability(argv[3], &failure_prob) != 0) {
        fprintf(stderr, "Error: Invalid failure probability '%s' (must be between 0.0 and 1.0)\n", argv[3]);
        return 1;
    }

    init_fault_injection(failure_prob, 0);
    session_table_init();

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 64) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("===============================================================\n");
    printf("IT305 Server (Part 2 Case 2: Resume)\n");
    printf("Port        : %d\n", port);
    printf("Root Dir    : %s\n", argv[2]);
    printf("Fault Prob  : %.4f\n", failure_prob);
    printf("===============================================================\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        ClientContext *ctx = (ClientContext *)malloc(sizeof(ClientContext));
        if (!ctx) {
            close(client_fd);
            continue;
        }
        ctx->client_fd = client_fd;
        strncpy(ctx->topics_root_dir, argv[2], sizeof(ctx->topics_root_dir) - 1);
        ctx->topics_root_dir[sizeof(ctx->topics_root_dir) - 1] = '\0';

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&tid, &attr, client_worker, ctx) != 0) {
            close(client_fd);
            free(ctx);
        }
        pthread_attr_destroy(&attr);
    }

    close(listen_fd);
    session_table_destroy();
    return 0;
}
