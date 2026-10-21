/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/protocol.h
 * Description: Binary Wire Protocol Framing Definitions and Interface.
 *
 * Phase: Phase 1 (Core Wire Framing & Baseline Server)
 * Status: FROZEN v1.0
 *
 * Source of Truth Hierarchy:
 *   - Tier 1: IT305 Assignment Specification (Folder layout, CLI arguments)
 *   - Tier 2: Instructor Clarifications (Framing concepts)
 *   - Tier 3: Approved Design Document Group 071_072 (§6.1-§6.5)
 *   - Tier 4: Master Execution Roadmap (PART2_TASKS_ROADMAP.md §4.1)
 * ============================================================================
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------------
 * Protocol Magic and Version Constants
 * ---------------------------------------------------------------------------- */

/**
 * PROTOCOL_MAGIC: Fixed 2-byte marker ASCII 'T', 'F' (0x5446)
 * Magic numbers allow immediate detection of protocol mismatches
 * or random non-protocol network probes before allocating any buffers.
 */
#define PROTOCOL_MAGIC 0x5446

/**
 * PROTOCOL_VERSION: Fixed 1-byte wire protocol version (0x01)
 */
#define PROTOCOL_VERSION 0x01

/**
 * DEFAULT_CHUNK_SIZE: Bounded 64 KB file payload chunk.
 * Transmitting files in 64 KB chunks keeps socket latency low, prevents
 * allocating entire large files into RAM, and establishes checkpoint bounds.
 */
#define DEFAULT_CHUNK_SIZE (64 * 1024)

/**
 * MAX_PAYLOAD_SIZE: Hard ceiling of 16 MB on any single frame payload.
 * Prevents memory exhaustion and integer wrap attacks caused by corrupted
 * 64-bit length fields.
 */
#define MAX_PAYLOAD_SIZE (16 * 1024 * 1024)

/* ----------------------------------------------------------------------------
 * Frame Header Flags
 * ---------------------------------------------------------------------------- */
#define FLAG_NONE       0x0000
#define FLAG_CONFLICT   0x0001  /* Bit 0: Set on resume conflict/rewind */
#define FLAG_LAST       0x0002  /* Bit 1: Set on final chunk of work unit */

/* ----------------------------------------------------------------------------
 * Standard Error Codes (MsgTopicErrorPayload)
 * ---------------------------------------------------------------------------- */
#define ERR_BAD_REQUEST      400 /* Malformed request payload or invalid syntax */
#define ERR_FORBIDDEN        403 /* Path traversal attempt (.., /, \\) */
#define ERR_TOPIC_NOT_FOUND  404 /* Topic directory does not exist */
#define ERR_INTERNAL         500 /* Server file-system I/O or resource error */

/* ----------------------------------------------------------------------------
 * Fixed 16-Byte Binary Frame Header
 * ---------------------------------------------------------------------------- */

/**
 * FrameHeader: Exactly 16 bytes on wire (packed, big-endian).
 *
 * Wire Layout:
 *   [0x00 - 0x01] : uint16_t magic       (Must match PROTOCOL_MAGIC 0x5446)
 *   [0x02]        : uint8_t  version     (Must match PROTOCOL_VERSION 0x01)
 *   [0x03]        : uint8_t  type        (MessageType ID)
 *   [0x04 - 0x05] : uint16_t flags       (Bitwise flags, default 0x0000)
 *   [0x06 - 0x07] : uint16_t reserved    (Reserved padding, must be 0x0000)
 *   [0x08 - 0x0F] : uint64_t payload_len (Payload length in bytes, big-endian)
 *
 * TCP is a continuous byte-stream protocol without message boundaries.
 * We enforce a fixed 16-byte binary header so the receiver can ALWAYS read
 * exactly 16 bytes first to discover the exact length of the subsequent payload.
 */
typedef struct __attribute__((packed)) {
    uint16_t magic;        /* 0x00-0x01: Must match PROTOCOL_MAGIC (0x5446) */
    uint8_t  version;      /* 0x02:      Must match PROTOCOL_VERSION (0x01) */
    uint8_t  type;         /* 0x03:      Message Type identifier (MessageType enum) */
    uint16_t flags;        /* 0x04-0x05: Bitwise flags (FLAG_CONFLICT, FLAG_LAST) */
    uint16_t reserved;     /* 0x06-0x07: Reserved padding, must be 0x0000 */
    uint64_t payload_len;  /* 0x08-0x0F: Length of following payload in bytes */
} FrameHeader;

/* ----------------------------------------------------------------------------
 * Message Type Registry
 * ---------------------------------------------------------------------------- */
typedef enum {
    /* Baseline Protocol (Part 1 & Part 2 Case 1) */
    MSG_GET              = 0x01, /* Client -> Server: Request topic transfer */
    MSG_TOPIC_OK         = 0x02, /* Server -> Client: Topic accepted, manifest metadata */
    MSG_TOPIC_ERROR      = 0x03, /* Server -> Client: Topic invalid, not found, or traversal rejected */
    MSG_FILE_BEGIN       = 0x04, /* Server -> Client: Metadata for upcoming file stream */
    MSG_FILE_DATA        = 0x05, /* Server -> Client: Bounded raw binary file payload chunk */
    MSG_FILE_END         = 0x06, /* Server -> Client: Boundary confirming file stream complete */
    MSG_TRANSFER_END     = 0x07, /* Server -> Client: Signals entire topic transfer finished */

    /* Case 2 Session Management */
    MSG_SESSION_START    = 0x08, /* Client -> Server: Initiate new stateful session */
    MSG_SESSION_ACCEPT   = 0x09, /* Server -> Client: Return assigned session ID & fingerprint */
    MSG_CHUNK_ACK        = 0x0A, /* Client -> Server: Acknowledge chunk written to local disk */
    MSG_SESSION_RESUME   = 0x0B, /* Client -> Server: Request resumption with local checkpoint */
    MSG_RESUME_ACCEPT    = 0x0C, /* Server -> Client: Authoritative resumption point approved */
    MSG_RESUME_ERROR     = 0x0D, /* Server -> Client: Resume rejected (fingerprint mismatch/expired) */

    /* Case 2 Enhanced Parallel Connections */
    MSG_WORK_PLAN        = 0x0E, /* Server -> Client: Partition plan for parallel workers */
    MSG_WORK_ASSIGN      = 0x0F, /* Server -> Client: Assign specific unit (file/range) to connection */
    MSG_WORK_DATA        = 0x10, /* Server -> Client: Positional data chunk for assigned unit */
    MSG_WORK_END         = 0x11, /* Server -> Client: Work unit completed */

    /* General Control */
    MSG_ERROR            = 0x1E, /* Bidirectional: General protocol violation or error */
    MSG_CLOSE            = 0x1F  /* Bidirectional: Clean connection termination */
} MessageType;

/* ----------------------------------------------------------------------------
 * Binary Payload Structures
 * Note: All multi-byte integers inside payloads are serialized in Network Byte Order
 * ---------------------------------------------------------------------------- */

/* MSG_GET (0x01) Payload */
typedef struct __attribute__((packed)) {
    char topic[128];               /* Null-terminated topic directory name */
} MsgGetPayload;

/* MSG_TOPIC_OK (0x02) Payload */
typedef struct __attribute__((packed)) {
    uint32_t file_count;           /* Total number of regular files in topic */
    uint32_t manifest_fingerprint; /* CRC32 checksum of sorted file names + sizes */
    uint64_t total_topic_bytes;    /* Sum of all file sizes in bytes */
} MsgTopicOkPayload;

/* MSG_TOPIC_ERROR (0x03) Payload */
typedef struct __attribute__((packed)) {
    uint32_t error_code;           /* 404: Not Found, 403: Security Violation, etc. */
    char error_msg[256];           /* Human-readable diagnostic message (null-terminated) */
} MsgTopicErrorPayload;

/* MSG_FILE_BEGIN (0x04) Payload */
typedef struct __attribute__((packed)) {
    uint32_t file_index;           /* 0-based lexicographical index */
    uint64_t file_size;            /* Total file size in bytes */
    uint64_t start_offset;         /* Offset from which server starts sending (0 or resume) */
    char filename[256];            /* File name without directories (null-terminated) */
} MsgFileBeginPayload;

/* MSG_FILE_END (0x06) Payload */
typedef struct __attribute__((packed)) {
    uint32_t file_index;           /* Confirmed file index */
    uint64_t final_offset;         /* Offset reached upon completion */
} MsgFileEndPayload;

/* MSG_SESSION_START (0x08) Payload */
typedef struct __attribute__((packed)) {
    char topic[128];               /* Requested topic */
    uint32_t requested_conns;      /* Requested parallel TCP connections (1 for default) */
} MsgSessionStartPayload;

/* MSG_SESSION_ACCEPT (0x09) Payload */
typedef struct __attribute__((packed)) {
    uint64_t session_id;           /* Random 64-bit non-zero session ID */
    uint32_t manifest_fingerprint; /* Manifest integrity fingerprint */
    uint32_t file_count;           /* Total topic files */
} MsgSessionAcceptPayload;

/* MSG_CHUNK_ACK (0x0A) Payload */
typedef struct __attribute__((packed)) {
    uint64_t session_id;           /* Active session ID */
    uint32_t file_index;           /* File index of chunk */
    uint64_t end_offset;           /* Byte offset reached in file after writing chunk */
    uint32_t seq_num;              /* Chunk sequence number */
} MsgChunkAckPayload;

/* MSG_SESSION_RESUME (0x0B) Payload */
typedef struct __attribute__((packed)) {
    uint64_t session_id;           /* Previous session ID */
    char topic[128];               /* Topic name verification */
    uint32_t manifest_fingerprint; /* Local stored manifest fingerprint */
    uint32_t proposed_file_index;  /* Client's last acknowledged file index */
    uint64_t proposed_file_offset; /* Client's last acknowledged byte offset */
} MsgSessionResumePayload;

/* MSG_RESUME_ACCEPT (0x0C) Payload */
typedef struct __attribute__((packed)) {
    uint64_t session_id;           /* Active session ID */
    uint32_t authoritative_file_idx;    /* Server's authoritative resume file index */
    uint64_t authoritative_file_offset; /* Server's authoritative resume byte offset */
    uint16_t resume_flags;              /* Bit 0: 1 = Conflict/Rewind forced */
} MsgResumeAcceptPayload;

/* ----------------------------------------------------------------------------
 * Byte-Order / Endianness Conversion Helper Functions
 * ---------------------------------------------------------------------------- */

/* 64-bit integer endianness conversion */
uint64_t proto_hton64(uint64_t val);
uint64_t proto_ntoh64(uint64_t val);

/* Payload endianness helpers for multi-byte structs */
void proto_hton_topic_ok(MsgTopicOkPayload *dst, const MsgTopicOkPayload *src);
void proto_ntoh_topic_ok(MsgTopicOkPayload *dst, const MsgTopicOkPayload *src);

void proto_hton_topic_error(MsgTopicErrorPayload *dst, const MsgTopicErrorPayload *src);
void proto_ntoh_topic_error(MsgTopicErrorPayload *dst, const MsgTopicErrorPayload *src);

void proto_hton_file_begin(MsgFileBeginPayload *dst, const MsgFileBeginPayload *src);
void proto_ntoh_file_begin(MsgFileBeginPayload *dst, const MsgFileBeginPayload *src);

void proto_hton_file_end(MsgFileEndPayload *dst, const MsgFileEndPayload *src);
void proto_ntoh_file_end(MsgFileEndPayload *dst, const MsgFileEndPayload *src);

void proto_hton_chunk_ack(MsgChunkAckPayload *dst, const MsgChunkAckPayload *src);
void proto_ntoh_chunk_ack(MsgChunkAckPayload *dst, const MsgChunkAckPayload *src);

void proto_hton_session_accept(MsgSessionAcceptPayload *dst, const MsgSessionAcceptPayload *src);
void proto_ntoh_session_accept(MsgSessionAcceptPayload *dst, const MsgSessionAcceptPayload *src);

void proto_hton_session_resume(MsgSessionResumePayload *dst, const MsgSessionResumePayload *src);
void proto_ntoh_session_resume(MsgSessionResumePayload *dst, const MsgSessionResumePayload *src);

void proto_hton_resume_accept(MsgResumeAcceptPayload *dst, const MsgResumeAcceptPayload *src);
void proto_ntoh_resume_accept(MsgResumeAcceptPayload *dst, const MsgResumeAcceptPayload *src);

/* ----------------------------------------------------------------------------
 * Network I/O Framing Interface
 * ---------------------------------------------------------------------------- */

/**
 * send_all:
 * Transmits exactly `len` bytes from buffer `buf` to stream socket `fd`.
 * Loops on partial sends and retries on EINTR.
 *
 * Parameters:
 *   fd  : Connected socket descriptor.
 *   buf : Pointer to data to send.
 *   len : Exact number of bytes to transmit.
 *
 * Return value:
 *    0 : Success (all `len` bytes sent).
 *   -1 : Fatal error or remote socket closure before completion (sets errno).
 */
int send_all(int fd, const void *buf, size_t len);

/**
 * recv_all:
 * Receives exactly `len` bytes into buffer `buf` from stream socket `fd`.
 * Loops on partial receives and retries on EINTR.
 *
 * Parameters:
 *   fd  : Connected socket descriptor.
 *   buf : Pointer to destination buffer (must be at least `len` bytes).
 *   len : Exact number of bytes to receive.
 *
 * Return value:
 *    0 : Success (all `len` bytes received).
 *    1 : Orderly remote socket closure (clean EOF) before ANY byte was read.
 *   -1 : Fatal socket error, or unexpected EOF received mid-transfer (truncated).
 */
int recv_all(int fd, void *buf, size_t len);

/**
 * send_frame:
 * Encodes a 16-byte FrameHeader in network byte order and transmits
 * the header followed by `payload_len` bytes from `payload` over socket `fd`.
 *
 * Parameters:
 *   fd          : Connected socket descriptor.
 *   type        : MessageType identifier.
 *   flags       : Header flags (e.g. FLAG_NONE, FLAG_LAST).
 *   payload     : Pointer to payload buffer (may be NULL if payload_len == 0).
 *   payload_len : Length of payload in bytes.
 *
 * Return value:
 *    0 : Success (header and entire payload transmitted).
 *   -1 : Socket error, invalid parameters, or payload_len > MAX_PAYLOAD_SIZE.
 */
int send_frame(int fd, uint8_t type, uint16_t flags, const void *payload, uint64_t payload_len);

/**
 * recv_frame_header:
 * Reads a 16-byte FrameHeader from socket `fd`, converts all fields from
 * network byte order to host byte order, and performs structural validation.
 *
 * Validation checks performed:
 *   - Exactly 16 bytes received without mid-header EOF.
 *   - hdr->magic == PROTOCOL_MAGIC (0x5446).
 *   - hdr->version == PROTOCOL_VERSION (0x01).
 *   - hdr->reserved == 0x0000.
 *   - hdr->payload_len <= MAX_PAYLOAD_SIZE.
 *
 * Parameters:
 *   fd  : Connected socket descriptor.
 *   hdr : Pointer to FrameHeader struct to populate (in host byte order).
 *
 * Return value:
 *    0 : Success (valid header received and decoded).
 *    1 : Orderly remote closure (clean EOF before receiving any header bytes).
 *   -1 : Socket error, mid-header truncated EOF, or header validation failure.
 */
int recv_frame_header(int fd, FrameHeader *hdr);

/**
 * recv_frame_payload:
 * Reads exactly `hdr->payload_len` bytes into caller-provided buffer `buf`.
 *
 * Buffer ownership & safety:
 *   - Buffer is allocated and owned exclusively by the CALLER.
 *   - If hdr->payload_len == 0, returns 0 immediately without reading socket.
 *   - If hdr->payload_len > max_buf_size, returns -1 (buffer overflow protection).
 *
 * Parameters:
 *   fd           : Connected socket descriptor.
 *   hdr          : Pointer to decoded FrameHeader (from recv_frame_header).
 *   buf          : Caller-owned destination buffer.
 *   max_buf_size : Capacity of destination buffer in bytes.
 *
 * Return value:
 *    0 : Success (entire payload received).
 *   -1 : Socket error, unexpected EOF mid-payload, or buffer too small.
 */
int recv_frame_payload(int fd, const FrameHeader *hdr, void *buf, size_t max_buf_size);

/**
 * recv_frame_payload_alloc:
 * Dynamically allocates a buffer of `hdr->payload_len` bytes using malloc()
 * and reads the entire payload into it.
 *
 * Buffer ownership contract:
 *   - If hdr->payload_len == 0, sets *out_buf = NULL and returns 0.
 *   - If successful and payload_len > 0, the CALLER OWNS *out_buf and
 *     MUST release it via free().
 *   - On error (-1 returned), *out_buf is set to NULL and any memory freed.
 *
 * Parameters:
 *   fd      : Connected socket descriptor.
 *   hdr     : Pointer to decoded FrameHeader.
 *   out_buf : Address of pointer to receive allocated payload buffer.
 *
 * Return value:
 *    0 : Success.
 *   -1 : Memory allocation failure, socket error, or invalid arguments.
 */
int recv_frame_payload_alloc(int fd, const FrameHeader *hdr, void **out_buf);

/**
 * is_valid_message_type:
 * Validates whether the given type ID is a recognized protocol message type.
 */
bool is_valid_message_type(uint8_t type);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
