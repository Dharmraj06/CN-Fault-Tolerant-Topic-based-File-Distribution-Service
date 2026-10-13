/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/protocol.c
 * Description: Wire Protocol Framing and Byte-Stream Socket Helpers.
 *
 * Phase: Phase 1 (Core Wire Framing & Baseline Server)
 * Status: FROZEN v1.0
 *
 * Source of Truth Hierarchy:
 *   - Tier 1: IT305 Assignment Specification (§Part 1)
 *   - Tier 2: Instructor Clarifications (§Page 2-3)
 *   - Tier 3: Approved Design Document Group 071_072 (§6.1-§6.5)
 *   - Tier 4: Master Execution Roadmap (PART2_TASKS_ROADMAP.md §4.1)
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(__linux__) || defined(__GLIBC__)
#  include <endian.h>
#endif

/* ----------------------------------------------------------------------------
 * 64-Bit Integer Endianness Conversions
 * ---------------------------------------------------------------------------- */

uint64_t proto_hton64(uint64_t val) {
#if defined(htobe64)
    return htobe64(val);
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return (((uint64_t)htonl((uint32_t)(val & 0xFFFFFFFFULL))) << 32) |
           ((uint64_t)htonl((uint32_t)(val >> 32)));
#else
    /* Host is Big-Endian */
    return val;
#endif
}

uint64_t proto_ntoh64(uint64_t val) {
#if defined(be64toh)
    return be64toh(val);
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return (((uint64_t)ntohl((uint32_t)(val & 0xFFFFFFFFULL))) << 32) |
           ((uint64_t)ntohl((uint32_t)(val >> 32)));
#else
    /* Host is Big-Endian */
    return val;
#endif
}

/* ----------------------------------------------------------------------------
 * Multi-Byte Payload Endianness Helpers
 * ---------------------------------------------------------------------------- */

void proto_hton_topic_ok(MsgTopicOkPayload *dst, const MsgTopicOkPayload *src) {
    if (!dst || !src) return;
    dst->file_count           = htonl(src->file_count);
    dst->manifest_fingerprint = htonl(src->manifest_fingerprint);
    dst->total_topic_bytes    = proto_hton64(src->total_topic_bytes);
}

void proto_ntoh_topic_ok(MsgTopicOkPayload *dst, const MsgTopicOkPayload *src) {
    if (!dst || !src) return;
    dst->file_count           = ntohl(src->file_count);
    dst->manifest_fingerprint = ntohl(src->manifest_fingerprint);
    dst->total_topic_bytes    = proto_ntoh64(src->total_topic_bytes);
}

void proto_hton_topic_error(MsgTopicErrorPayload *dst, const MsgTopicErrorPayload *src) {
    if (!dst || !src) return;
    dst->error_code = htonl(src->error_code);
    memcpy(dst->error_msg, src->error_msg, sizeof(dst->error_msg));
    dst->error_msg[sizeof(dst->error_msg) - 1] = '\0';
}

void proto_ntoh_topic_error(MsgTopicErrorPayload *dst, const MsgTopicErrorPayload *src) {
    if (!dst || !src) return;
    dst->error_code = ntohl(src->error_code);
    memcpy(dst->error_msg, src->error_msg, sizeof(dst->error_msg));
    dst->error_msg[sizeof(dst->error_msg) - 1] = '\0';
}

void proto_hton_file_begin(MsgFileBeginPayload *dst, const MsgFileBeginPayload *src) {
    if (!dst || !src) return;
    dst->file_index   = htonl(src->file_index);
    dst->file_size    = proto_hton64(src->file_size);
    dst->start_offset = proto_hton64(src->start_offset);
    memcpy(dst->filename, src->filename, sizeof(dst->filename));
    dst->filename[sizeof(dst->filename) - 1] = '\0';
}

void proto_ntoh_file_begin(MsgFileBeginPayload *dst, const MsgFileBeginPayload *src) {
    if (!dst || !src) return;
    dst->file_index   = ntohl(src->file_index);
    dst->file_size    = proto_ntoh64(src->file_size);
    dst->start_offset = proto_ntoh64(src->start_offset);
    memcpy(dst->filename, src->filename, sizeof(dst->filename));
    dst->filename[sizeof(dst->filename) - 1] = '\0';
}

void proto_hton_file_end(MsgFileEndPayload *dst, const MsgFileEndPayload *src) {
    if (!dst || !src) return;
    dst->file_index   = htonl(src->file_index);
    dst->final_offset = proto_hton64(src->final_offset);
}

void proto_ntoh_file_end(MsgFileEndPayload *dst, const MsgFileEndPayload *src) {
    if (!dst || !src) return;
    dst->file_index   = ntohl(src->file_index);
    dst->final_offset = proto_ntoh64(src->final_offset);
}

void proto_hton_chunk_ack(MsgChunkAckPayload *dst, const MsgChunkAckPayload *src) {
    if (!dst || !src) return;
    dst->session_id  = proto_hton64(src->session_id);
    dst->file_index  = htonl(src->file_index);
    dst->end_offset  = proto_hton64(src->end_offset);
    dst->seq_num     = htonl(src->seq_num);
}

void proto_ntoh_chunk_ack(MsgChunkAckPayload *dst, const MsgChunkAckPayload *src) {
    if (!dst || !src) return;
    dst->session_id  = proto_ntoh64(src->session_id);
    dst->file_index  = ntohl(src->file_index);
    dst->end_offset  = proto_ntoh64(src->end_offset);
    dst->seq_num     = ntohl(src->seq_num);
}

void proto_hton_session_accept(MsgSessionAcceptPayload *dst, const MsgSessionAcceptPayload *src) {
    if (!dst || !src) return;
    dst->session_id           = proto_hton64(src->session_id);
    dst->manifest_fingerprint = htonl(src->manifest_fingerprint);
    dst->file_count           = htonl(src->file_count);
}

void proto_ntoh_session_accept(MsgSessionAcceptPayload *dst, const MsgSessionAcceptPayload *src) {
    if (!dst || !src) return;
    dst->session_id           = proto_ntoh64(src->session_id);
    dst->manifest_fingerprint = ntohl(src->manifest_fingerprint);
    dst->file_count           = ntohl(src->file_count);
}

void proto_hton_session_resume(MsgSessionResumePayload *dst, const MsgSessionResumePayload *src) {
    if (!dst || !src) return;
    dst->session_id            = proto_hton64(src->session_id);
    memcpy(dst->topic, src->topic, sizeof(dst->topic));
    dst->topic[sizeof(dst->topic) - 1] = '\0';
    dst->manifest_fingerprint  = htonl(src->manifest_fingerprint);
    dst->proposed_file_index   = htonl(src->proposed_file_index);
    dst->proposed_file_offset  = proto_hton64(src->proposed_file_offset);
}

void proto_ntoh_session_resume(MsgSessionResumePayload *dst, const MsgSessionResumePayload *src) {
    if (!dst || !src) return;
    dst->session_id            = proto_ntoh64(src->session_id);
    memcpy(dst->topic, src->topic, sizeof(dst->topic));
    dst->topic[sizeof(dst->topic) - 1] = '\0';
    dst->manifest_fingerprint  = ntohl(src->manifest_fingerprint);
    dst->proposed_file_index   = ntohl(src->proposed_file_index);
    dst->proposed_file_offset  = proto_ntoh64(src->proposed_file_offset);
}

void proto_hton_resume_accept(MsgResumeAcceptPayload *dst, const MsgResumeAcceptPayload *src) {
    if (!dst || !src) return;
    dst->session_id                 = proto_hton64(src->session_id);
    dst->authoritative_file_idx     = htonl(src->authoritative_file_idx);
    dst->authoritative_file_offset  = proto_hton64(src->authoritative_file_offset);
    dst->resume_flags               = htons(src->resume_flags);
}

void proto_ntoh_resume_accept(MsgResumeAcceptPayload *dst, const MsgResumeAcceptPayload *src) {
    if (!dst || !src) return;
    dst->session_id                 = proto_ntoh64(src->session_id);
    dst->authoritative_file_idx     = ntohl(src->authoritative_file_idx);
    dst->authoritative_file_offset  = proto_ntoh64(src->authoritative_file_offset);
    dst->resume_flags               = ntohs(src->resume_flags);
}

/* ----------------------------------------------------------------------------
 * Message Type Validation
 * ---------------------------------------------------------------------------- */

bool is_valid_message_type(uint8_t type) {
    switch (type) {
        case MSG_GET:
        case MSG_TOPIC_OK:
        case MSG_TOPIC_ERROR:
        case MSG_FILE_BEGIN:
        case MSG_FILE_DATA:
        case MSG_FILE_END:
        case MSG_TRANSFER_END:
        case MSG_SESSION_START:
        case MSG_SESSION_ACCEPT:
        case MSG_CHUNK_ACK:
        case MSG_SESSION_RESUME:
        case MSG_RESUME_ACCEPT:
        case MSG_RESUME_ERROR:
        case MSG_WORK_PLAN:
        case MSG_WORK_ASSIGN:
        case MSG_WORK_DATA:
        case MSG_WORK_END:
        case MSG_ERROR:
        case MSG_CLOSE:
            return true;
        default:
            return false;
    }
}

/* ----------------------------------------------------------------------------
 * Robust Byte-Stream Socket I/O
 * ---------------------------------------------------------------------------- */

int send_all(int fd, const void *buf, size_t len) {
    if (fd < 0 || (len > 0 && buf == NULL)) {
        errno = EINVAL;
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    const char *ptr = (const char *)buf;
    size_t total_sent = 0;

    while (total_sent < len) {
        /*
         * Under Linux, writing to a socket whose remote end has
         * closed triggers a SIGPIPE signal, which kills the process by default.
         * Using MSG_NOSIGNAL ensures send() instead returns EPIPE.
         */
#if defined(MSG_NOSIGNAL)
        ssize_t n = send(fd, ptr + total_sent, len - total_sent, MSG_NOSIGNAL);
#else
        ssize_t n = send(fd, ptr + total_sent, len - total_sent, 0);
#endif
        if (n < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted by signal, retry immediately */
            }
            return -1; /* Fatal socket write error (e.g., EPIPE, ECONNRESET) */
        }
        if (n == 0) {
            /* Connection terminated prematurely by peer */
            errno = EPIPE;
            return -1;
        }
        total_sent += (size_t)n;
    }

    return 0;
}

int recv_all(int fd, void *buf, size_t len) {
    if (fd < 0 || (len > 0 && buf == NULL)) {
        errno = EINVAL;
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    char *ptr = (char *)buf;
    size_t total_received = 0;

    while (total_received < len) {
        ssize_t n = recv(fd, ptr + total_received, len - total_received, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted by signal, retry immediately */
            }
            return -1; /* Fatal socket read error (e.g., ECONNRESET) */
        }
        if (n == 0) {
            /* Peer executed an orderly shutdown / close */
            if (total_received == 0) {
                return 1; /* Clean EOF before any byte was read */
            } else {
                /*
                 * Premature EOF mid-frame: peer closed before fulfilling
                 * the expected byte count. This is a fatal framing error.
                 */
                errno = ECONNABORTED;
                return -1;
            }
        }
        total_received += (size_t)n;
    }

    return 0;
}

/* ----------------------------------------------------------------------------
 * Wire Framing Interface
 * ---------------------------------------------------------------------------- */

int send_frame(int fd, uint8_t type, uint16_t flags, const void *payload, uint64_t payload_len) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (payload_len > MAX_PAYLOAD_SIZE) {
        errno = EMSGSIZE;
        return -1;
    }
    if (payload_len > 0 && payload == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* Prepare fixed 16-byte FrameHeader in Network Byte Order */
    FrameHeader wire_hdr;
    wire_hdr.magic       = htons(PROTOCOL_MAGIC);
    wire_hdr.version     = PROTOCOL_VERSION;
    wire_hdr.type        = type;
    wire_hdr.flags       = htons(flags);
    wire_hdr.reserved    = htons(0x0000);
    wire_hdr.payload_len = proto_hton64(payload_len);

    /* Transmit the 16-byte header first */
    if (send_all(fd, &wire_hdr, sizeof(wire_hdr)) != 0) {
        return -1;
    }

    /* Transmit payload bytes if present */
    if (payload_len > 0) {
        if (send_all(fd, payload, (size_t)payload_len) != 0) {
            return -1;
        }
    }

    return 0;
}

int recv_frame_header(int fd, FrameHeader *hdr) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (hdr == NULL) {
        errno = EINVAL;
        return -1;
    }

    FrameHeader wire_hdr;
    int rc = recv_all(fd, &wire_hdr, sizeof(wire_hdr));
    if (rc != 0) {
        /* Returns 1 for clean EOF, or -1 for error/truncated read */
        return rc;
    }

    /* Convert received network bytes to host byte order */
    hdr->magic       = ntohs(wire_hdr.magic);
    hdr->version     = wire_hdr.version;
    hdr->type        = wire_hdr.type;
    hdr->flags       = ntohs(wire_hdr.flags);
    hdr->reserved    = ntohs(wire_hdr.reserved);
    hdr->payload_len = proto_ntoh64(wire_hdr.payload_len);

    /* Enforce strict protocol header validation */
    if (hdr->magic != PROTOCOL_MAGIC) {
        /* Protocol mismatch: immediately reject */
        errno = EPROTO;
        return -1;
    }
    if (hdr->version != PROTOCOL_VERSION) {
        /* Version mismatch: reject */
        errno = EPROTONOSUPPORT;
        return -1;
    }
    if (hdr->reserved != 0x0000) {
        /* Non-zero reserved bits: violation of wire contract */
        errno = EPROTO;
        return -1;
    }
    if (hdr->payload_len > MAX_PAYLOAD_SIZE) {
        /* Unbounded payload length exceeds 16 MB ceiling */
        errno = EMSGSIZE;
        return -1;
    }

    return 0;
}

int recv_frame_payload(int fd, const FrameHeader *hdr, void *buf, size_t max_buf_size) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (hdr == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (hdr->payload_len == 0) {
        return 0; /* Empty payload: successfully processed */
    }
    if (buf == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (hdr->payload_len > max_buf_size) {
        /* Caller buffer capacity is smaller than declared payload */
        errno = ENOBUFS;
        return -1;
    }

    int rc = recv_all(fd, buf, (size_t)hdr->payload_len);
    if (rc != 0) {
        /* Premature EOF mid-payload or socket error is a fatal error */
        return -1;
    }

    return 0;
}

int recv_frame_payload_alloc(int fd, const FrameHeader *hdr, void **out_buf) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (hdr == NULL || out_buf == NULL) {
        errno = EINVAL;
        return -1;
    }

    *out_buf = NULL;

    if (hdr->payload_len == 0) {
        return 0; /* Empty payload: no memory allocated */
    }
    if (hdr->payload_len > MAX_PAYLOAD_SIZE) {
        errno = EMSGSIZE;
        return -1;
    }

    void *mem = malloc((size_t)hdr->payload_len);
    if (mem == NULL) {
        errno = ENOMEM;
        return -1;
    }

    int rc = recv_all(fd, mem, (size_t)hdr->payload_len);
    if (rc != 0) {
        /* Clean up allocated buffer on read failure to prevent memory leak */
        free(mem);
        *out_buf = NULL;
        return -1;
    }

    *out_buf = mem;
    return 0;
}
