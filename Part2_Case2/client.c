/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: Part2_Case2/client.c
 * Description: Simplified Topic File Client with Checkpoint-Based Resumption.
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_RETRIES 50

/* Checkpoint file persistence helpers */
static void get_checkpoint_path(const char *output_dir, const char *topic, char *buf, size_t size) {
    snprintf(buf, size, "%s/%s/.session_checkpoint", output_dir, topic);
}

static void save_checkpoint(const char *out_dir, const char *topic,
                            uint64_t session_id, uint32_t fp_val,
                            uint32_t fidx, uint64_t offset) {
    char path[2048];
    get_checkpoint_path(out_dir, topic, path, sizeof(path));
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "%lu %u %u %lu\n",
                (unsigned long)session_id, (unsigned int)fp_val,
                (unsigned int)fidx, (unsigned long)offset);
        fclose(fp);
    }
}

static bool load_checkpoint(const char *out_dir, const char *topic,
                            uint64_t *session_id, uint32_t *fp_val,
                            uint32_t *fidx, uint64_t *offset) {
    char path[2048];
    get_checkpoint_path(out_dir, topic, path, sizeof(path));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return false;
    }
    unsigned long sid = 0, off = 0;
    unsigned int f = 0, idx = 0;
    if (fscanf(fp, "%lu %u %u %lu", &sid, &f, &idx, &off) == 4 && sid != 0) {
        *session_id = (uint64_t)sid;
        *fp_val = (uint32_t)f;
        *fidx = (uint32_t)idx;
        *offset = (uint64_t)off;
        fclose(fp);
        return true;
    }
    fclose(fp);
    return false;
}

static void remove_checkpoint(const char *out_dir, const char *topic) {
    char path[2048];
    get_checkpoint_path(out_dir, topic, path, sizeof(path));
    remove(path);
}

/* Connect TCP socket to server */
static int connect_server(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <topic> <output_dir>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    const char *topic = argv[3];
    const char *output_dir = argv[4];

    /* Prepare destination directory */
    char topic_dir[2048];
    snprintf(topic_dir, sizeof(topic_dir), "%s/%s", output_dir, topic);
    create_directory_recursive(topic_dir);

    uint64_t session_id = 0;
    uint32_t fingerprint = 0;
    uint32_t current_file_idx = 0;
    uint64_t current_file_offset = 0;

    /* Restore prior local checkpoint if resuming */
    if (load_checkpoint(output_dir, topic, &session_id, &fingerprint, &current_file_idx, &current_file_offset)) {
        printf("[CLIENT] Restored existing checkpoint: Session 0x%016lX, File %u, Offset %lu bytes\n",
               (unsigned long)session_id, current_file_idx, (unsigned long)current_file_offset);
    }

    useconds_t backoff = 200000; /* 200 ms */

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        printf("[CLIENT] ---> Transfer Attempt #%d for topic '%s' (session=0x%016lX, file=%u, off=%lu)...\n",
               attempt, topic, (unsigned long)session_id, current_file_idx, (unsigned long)current_file_offset);

        int sock_fd = connect_server(server_ip, server_port);
        if (sock_fd < 0) {
            fprintf(stderr, "[CLIENT] Failed to connect to %s:%d\n", server_ip, server_port);
            usleep(backoff);
            backoff = (backoff * 2 > 2000000) ? 2000000 : backoff * 2;
            continue;
        }

        if (session_id == 0) {
            /* Initial session request */
            printf("[CLIENT] Sending MSG_SESSION_START for topic '%s'...\n", topic);
            MsgSessionStartPayload req;
            memset(&req, 0, sizeof(req));
            strncpy(req.topic, topic, sizeof(req.topic) - 1);
            req.requested_conns = htonl(1);
            if (send_frame(sock_fd, MSG_SESSION_START, FLAG_NONE, &req, sizeof(req)) != 0) {
                close(sock_fd);
                continue;
            }

            FrameHeader resp_hdr;
            if (recv_frame_header(sock_fd, &resp_hdr) != 0 || resp_hdr.type != MSG_SESSION_ACCEPT) {
                fprintf(stderr, "[CLIENT] Server rejected session start\n");
                close(sock_fd);
                return 1;
            }

            MsgSessionAcceptPayload wire_acc, acc;
            recv_frame_payload(sock_fd, &resp_hdr, &wire_acc, sizeof(wire_acc));
            proto_ntoh_session_accept(&acc, &wire_acc);

            session_id = acc.session_id;
            fingerprint = acc.manifest_fingerprint;
            printf("[CLIENT] Server created Session 0x%016lX: %u files, fingerprint 0x%08X\n",
                   (unsigned long)session_id, acc.file_count, fingerprint);
            current_file_idx = 0;
            current_file_offset = 0;

        } else {
            /* Resume existing session */
            printf("[CLIENT] Requesting RESUME for Session 0x%016lX at [file %u, offset %lu bytes]...\n",
                   (unsigned long)session_id, current_file_idx, (unsigned long)current_file_offset);

            MsgSessionResumePayload resume_req;
            memset(&resume_req, 0, sizeof(resume_req));
            resume_req.session_id = session_id;
            strncpy(resume_req.topic, topic, sizeof(resume_req.topic) - 1);
            resume_req.manifest_fingerprint = fingerprint;
            resume_req.proposed_file_index = current_file_idx;
            resume_req.proposed_file_offset = current_file_offset;

            MsgSessionResumePayload wire_resume;
            proto_hton_session_resume(&wire_resume, &resume_req);
            if (send_frame(sock_fd, MSG_SESSION_RESUME, FLAG_NONE, &wire_resume, sizeof(wire_resume)) != 0) {
                close(sock_fd);
                continue;
            }

            FrameHeader resp_hdr;
            if (recv_frame_header(sock_fd, &resp_hdr) != 0) {
                close(sock_fd);
                continue;
            }

            if (resp_hdr.type == MSG_RESUME_ERROR) {
                MsgTopicErrorPayload wire_err, err;
                recv_frame_payload(sock_fd, &resp_hdr, &wire_err, sizeof(wire_err));
                proto_ntoh_topic_error(&err, &wire_err);
                err.error_msg[sizeof(err.error_msg) - 1] = '\0';
                fprintf(stderr, "[CLIENT] Server rejected resume (code %u): %s\n", err.error_code, err.error_msg);
                remove_checkpoint(output_dir, topic);
                close(sock_fd);
                return 1;
            }

            if (resp_hdr.type != MSG_RESUME_ACCEPT) {
                fprintf(stderr, "[CLIENT] Unexpected resume response type 0x%02X\n", resp_hdr.type);
                close(sock_fd);
                return 1;
            }

            MsgResumeAcceptPayload wire_acc, acc;
            recv_frame_payload(sock_fd, &resp_hdr, &wire_acc, sizeof(wire_acc));
            proto_ntoh_resume_accept(&acc, &wire_acc);

            printf("[CLIENT] Resume ACCEPTED: Authoritative Checkpoint [file %u, offset %lu bytes] (flags=0x%04X)\n",
                   acc.authoritative_file_idx, (unsigned long)acc.authoritative_file_offset, acc.resume_flags);

            current_file_idx = acc.authoritative_file_idx;
            current_file_offset = acc.authoritative_file_offset;
        }

        /* Streaming receive loop */
        int file_fd = -1;
        uint64_t file_written = 0;
        uint32_t seq = 0;
        bool disconnected = false;

        while (1) {
            FrameHeader hdr;
            if (recv_frame_header(sock_fd, &hdr) != 0) {
                printf("[CLIENT] Server closed connection (EOF) during transfer\n");
                disconnected = true;
                break;
            }

            if (hdr.type == MSG_FILE_BEGIN) {
                if (file_fd >= 0) close(file_fd);

                MsgFileBeginPayload wire_b, b;
                recv_frame_payload(sock_fd, &hdr, &wire_b, sizeof(wire_b));
                proto_ntoh_file_begin(&b, &wire_b);
                b.filename[sizeof(b.filename) - 1] = '\0';

                char dest_file[2048];
                snprintf(dest_file, sizeof(dest_file), "%s/%s/%s", output_dir, topic, b.filename);

                if (b.start_offset == 0) {
                    file_fd = open_file_for_writing(dest_file, true);
                    file_written = 0;
                } else {
                    file_fd = open_file_for_writing(dest_file, false);
                    if (file_fd >= 0) {
                        struct stat st;
                        if (fstat(file_fd, &st) == 0 && (uint64_t)st.st_size > b.start_offset) {
                            printf("[CLIENT] Local file contains %lu bytes > authoritative %lu bytes. Truncating excess...\n",
                                   (unsigned long)st.st_size, (unsigned long)b.start_offset);
                        }
                        truncate_and_seek(file_fd, b.start_offset);
                    }
                    file_written = b.start_offset;
                }

                current_file_idx = b.file_index;
                current_file_offset = file_written;
                seq = (uint32_t)(file_written / DEFAULT_CHUNK_SIZE);

                printf("[CLIENT] Streaming '%s' [%u] starting at offset %lu / %lu bytes\n",
                       b.filename, b.file_index, (unsigned long)b.start_offset, (unsigned long)b.file_size);

            } else if (hdr.type == MSG_FILE_DATA) {
                char chunk[DEFAULT_CHUNK_SIZE];
                size_t len = (size_t)hdr.payload_len;
                if (recv_frame_payload(sock_fd, &hdr, chunk, len) != 0) {
                    disconnected = true;
                    break;
                }

                if (file_fd >= 0) {
                    write_chunk_to_file(file_fd, chunk, len);
                    sync_file_to_disk(file_fd);
                }

                file_written += len;
                current_file_offset = file_written;

                /* Send MSG_CHUNK_ACK */
                MsgChunkAckPayload ack = { session_id, current_file_idx, file_written, ++seq };
                MsgChunkAckPayload wire_ack;
                proto_hton_chunk_ack(&wire_ack, &ack);
                if (send_frame(sock_fd, MSG_CHUNK_ACK, FLAG_NONE, &wire_ack, sizeof(wire_ack)) != 0) {
                    disconnected = true;
                    break;
                }

                save_checkpoint(output_dir, topic, session_id, fingerprint, current_file_idx, current_file_offset);

            } else if (hdr.type == MSG_FILE_END) {
                MsgFileEndPayload wire_e, e;
                recv_frame_payload(sock_fd, &hdr, &wire_e, sizeof(wire_e));
                proto_ntoh_file_end(&e, &wire_e);

                if (file_fd >= 0) {
                    close(file_fd);
                    file_fd = -1;
                }

                printf("[CLIENT] Completed file [%u]: %lu bytes verified\n",
                       e.file_index, (unsigned long)e.final_offset);

                current_file_idx = e.file_index + 1;
                current_file_offset = 0;
                save_checkpoint(output_dir, topic, session_id, fingerprint, current_file_idx, current_file_offset);

            } else if (hdr.type == MSG_TRANSFER_END) {
                printf("[CLIENT] Received MSG_TRANSFER_END: Entire topic transfer complete!\n");
                if (file_fd >= 0) close(file_fd);
                remove_checkpoint(output_dir, topic);
                close(sock_fd);

                printf("\n===============================================================\n");
                printf("[CLIENT] SUCCESS: Topic '%s' successfully transferred via Case 2 Resume!\n", topic);
                printf("[CLIENT] Assigned Session ID : 0x%016lX\n", (unsigned long)session_id);
                printf("===============================================================\n");
                return 0;

            } else {
                fprintf(stderr, "[CLIENT] Unexpected frame 0x%02X\n", hdr.type);
                disconnected = true;
                break;
            }
        }

        if (file_fd >= 0) {
            close(file_fd);
        }
        close(sock_fd);

        if (disconnected) {
            printf("[CLIENT] Connection severed mid-transfer. Preserving local checkpoint [file %u, offset %lu bytes]...\n",
                   current_file_idx, (unsigned long)current_file_offset);
            printf("[CLIENT] Backing off for %u ms before reconnecting...\n", (unsigned int)(backoff / 1000));
            usleep(backoff);
            backoff = (backoff * 2 > 2000000) ? 2000000 : backoff * 2;
        }
    }

    fprintf(stderr, "[CLIENT] Failed after %d attempts\n", MAX_RETRIES);
    return 1;
}
