#ifndef ENHANCED_PROTOCOL_H
#define ENHANCED_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../common/protocol.h"
#include "../common/topic_manager.h"

/*
 * Phase 5 payloads.
 * The existing common protocol framing remains unchanged.
 * All integer fields transmitted over TCP use network byte order.
 */

#define ENHANCED_MAX_CONNECTIONS 16
#define ENHANCED_MAX_UNITS 1024
#define ENHANCED_DATA_CHUNK (64U * 1024U)

/* Client -> server on each worker socket. */
typedef struct __attribute__((packed)) {
    uint64_t session_id;
    uint32_t worker_id;
} EnhancedWorkerHello;

/* Server -> client: manifest and work-plan summary. */
typedef struct __attribute__((packed)) {
    uint64_t session_id;
    uint32_t file_count;
    uint32_t unit_count;
    uint32_t manifest_fingerprint;
} EnhancedWorkPlanHeader;

/* Server -> client: metadata for one destination file with SHA-256 digest. */
typedef struct __attribute__((packed)) {
    uint32_t file_index;
    uint64_t file_size;
    char filename[MAX_FILENAME_LEN];
    char relative_path[MAX_PATH_BUFFER_LEN];
    uint8_t sha256[32];
} EnhancedFileMetadata;

/* Server -> worker: one exclusive byte range [start_offset, end_offset). */
typedef struct __attribute__((packed)) {
    uint32_t unit_id;
    uint32_t file_index;
    uint64_t start_offset;
    uint64_t end_offset;
    uint64_t committed_offset;
} EnhancedWorkAssignment;

/* Server -> worker: chunk bytes at an absolute file offset. */
typedef struct __attribute__((packed)) {
    uint32_t unit_id;
    uint32_t file_index;
    uint64_t file_offset;
    uint32_t data_length;
} EnhancedWorkDataHeader;

/* Worker -> server: durable progress checkpoint. */
typedef struct __attribute__((packed)) {
    uint64_t session_id;
    uint32_t unit_id;
    uint32_t file_index;
    uint64_t committed_offset;
} EnhancedWorkAck;

/* Convert a 32-bit field in place. */
static inline uint32_t enhanced_hton32(uint32_t value)
{
    return (uint32_t)htonl(value);
}

static inline uint32_t enhanced_ntoh32(uint32_t value)
{
    return (uint32_t)ntohl(value);
}

/* Convert a 64-bit field in place. */
static inline uint64_t enhanced_hton64(uint64_t value)
{
    return proto_hton64(value);
}

static inline uint64_t enhanced_ntoh64(uint64_t value)
{
    return proto_ntoh64(value);
}

#endif
