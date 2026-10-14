/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/session.h
 * Description: Simplified In-Memory Session Table Interface (Part 2 Case 2).
 * ============================================================================
 */

#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_SESSIONS 64
#define SESSION_DEFAULT_TTL 300

/* Status of a session in the table */
typedef enum {
    SESSION_UNUSED = 0,
    SESSION_ACTIVE,
    SESSION_COMPLETE
} SessionStatus;

/* Point-in-time snapshot of a session record */
typedef struct {
    uint64_t session_id;
    char topic[128];
    uint32_t manifest_fingerprint;
    uint32_t file_count;
    uint32_t current_file_index;
    uint64_t current_file_offset;
    SessionStatus status;
} SessionState;

/* Initialize and teardown session table */
int session_table_init(void);
void session_table_destroy(void);

/* Create a new session with unique non-zero 64-bit ID */
int session_create(const char *topic, uint32_t fingerprint, uint32_t file_count, uint64_t *out_session_id);

/* Safely copy session state by value (no raw pointers leaked) */
int session_get_snapshot(uint64_t session_id, SessionState *out_state);

/* Monotonically commit acknowledged chunk offset */
int session_commit_chunk(uint64_t session_id, uint32_t file_idx, uint64_t end_offset, uint64_t file_size);

/* Advance session to the next file upon completing current file */
int session_complete_file(uint64_t session_id, uint32_t file_idx);

/* Mark session as completely transferred */
int session_mark_complete(uint64_t session_id);

/* Validate client resume request against server authoritative checkpoint */
int session_validate_resume(uint64_t session_id,
                            const char *topic,
                            uint32_t fingerprint,
                            uint32_t proposed_file,
                            uint64_t proposed_offset,
                            uint32_t *out_auth_file,
                            uint64_t *out_auth_offset,
                            uint16_t *out_flags);

/* Remove stale sessions exceeding TTL */
int session_cleanup_stale(int ttl_seconds);

#endif /* SESSION_H */
