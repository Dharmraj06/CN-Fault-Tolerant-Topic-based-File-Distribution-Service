/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/session.c
 * Description: Simplified In-Memory Session Table Implementation (Part 2 Case 2).
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "session.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* Internal representation of a session slot */
typedef struct {
    bool in_use;
    uint64_t session_id;
    char topic[128];
    uint32_t manifest_fingerprint;
    uint32_t file_count;
    uint32_t current_file_index;
    uint64_t current_file_offset;
    SessionStatus status;
    time_t last_activity;
} SessionRecord;

/* Fixed-size in-memory session table and mutex */
static SessionRecord s_table[MAX_SESSIONS];
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Find slot by session ID under mutex */
static SessionRecord* find_slot_locked(uint64_t session_id) {
    if (session_id == 0) {
        return NULL;
    }
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (s_table[i].in_use && s_table[i].session_id == session_id) {
            return &s_table[i];
        }
    }
    return NULL;
}

/* Generate unique non-zero 64-bit session ID */
static uint64_t generate_session_id_locked(void) {
    for (int retry = 0; retry < 100; retry++) {
        uint64_t t = (uint64_t)time(NULL);
        uint64_t p = (uint64_t)getpid();
        uint64_t r1 = (uint64_t)rand();
        uint64_t r2 = (uint64_t)rand();
        uint64_t id = (t << 32) ^ (p << 16) ^ (r1 << 16) ^ r2;

        if (id == 0) {
            continue;
        }

        bool collision = false;
        for (int i = 0; i < MAX_SESSIONS; i++) {
            if (s_table[i].in_use && s_table[i].session_id == id) {
                collision = true;
                break;
            }
        }
        if (!collision) {
            return id;
        }
    }
    return 0;
}

int session_table_init(void) {
    pthread_mutex_lock(&s_mutex);
    memset(s_table, 0, sizeof(s_table));
    pthread_mutex_unlock(&s_mutex);
    return 0;
}

void session_table_destroy(void) {
    pthread_mutex_lock(&s_mutex);
    memset(s_table, 0, sizeof(s_table));
    pthread_mutex_unlock(&s_mutex);
}

int session_create(const char *topic, uint32_t fingerprint, uint32_t file_count, uint64_t *out_session_id) {
    if (!topic || !out_session_id || strlen(topic) == 0) {
        return -1;
    }

    pthread_mutex_lock(&s_mutex);

    int free_slot = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!s_table[i].in_use) {
            free_slot = i;
            break;
        }
    }

    if (free_slot < 0) {
        pthread_mutex_unlock(&s_mutex);
        return -1; /* Table capacity reached */
    }

    uint64_t id = generate_session_id_locked();
    if (id == 0) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    SessionRecord *slot = &s_table[free_slot];
    memset(slot, 0, sizeof(SessionRecord));
    slot->in_use = true;
    slot->session_id = id;
    strncpy(slot->topic, topic, sizeof(slot->topic) - 1);
    slot->manifest_fingerprint = fingerprint;
    slot->file_count = file_count;
    slot->current_file_index = 0;
    slot->current_file_offset = 0;
    slot->status = SESSION_ACTIVE;
    slot->last_activity = time(NULL);

    *out_session_id = id;

    pthread_mutex_unlock(&s_mutex);
    return 0;
}

int session_get_snapshot(uint64_t session_id, SessionState *out_state) {
    if (session_id == 0 || !out_state) {
        return -1;
    }

    pthread_mutex_lock(&s_mutex);

    SessionRecord *slot = find_slot_locked(session_id);
    if (!slot) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    out_state->session_id = slot->session_id;
    strncpy(out_state->topic, slot->topic, sizeof(out_state->topic) - 1);
    out_state->topic[sizeof(out_state->topic) - 1] = '\0';
    out_state->manifest_fingerprint = slot->manifest_fingerprint;
    out_state->file_count = slot->file_count;
    out_state->current_file_index = slot->current_file_index;
    out_state->current_file_offset = slot->current_file_offset;
    out_state->status = slot->status;

    pthread_mutex_unlock(&s_mutex);
    return 0;
}

int session_commit_chunk(uint64_t session_id, uint32_t file_idx, uint64_t end_offset, uint64_t file_size) {
    if (session_id == 0) {
        return -1;
    }

    pthread_mutex_lock(&s_mutex);

    SessionRecord *slot = find_slot_locked(session_id);
    if (!slot || slot->status != SESSION_ACTIVE) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    /* Verify file index match */
    if (file_idx != slot->current_file_index) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    /* Bounds check */
    if (file_size > 0 && end_offset > file_size) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    /* Regression check */
    if (end_offset < slot->current_file_offset) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    /* Duplicate ACK */
    if (end_offset == slot->current_file_offset) {
        slot->last_activity = time(NULL);
        pthread_mutex_unlock(&s_mutex);
        return 1;
    }

    /* Monotonic forward progress */
    slot->current_file_offset = end_offset;
    slot->last_activity = time(NULL);

    pthread_mutex_unlock(&s_mutex);
    return 0;
}

int session_complete_file(uint64_t session_id, uint32_t file_idx) {
    if (session_id == 0) {
        return -1;
    }

    pthread_mutex_lock(&s_mutex);

    SessionRecord *slot = find_slot_locked(session_id);
    if (!slot || slot->status != SESSION_ACTIVE) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    if (file_idx != slot->current_file_index) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    /* Advance to next file at offset 0 */
    slot->current_file_index = file_idx + 1;
    slot->current_file_offset = 0;
    slot->last_activity = time(NULL);

    pthread_mutex_unlock(&s_mutex);
    return 0;
}

int session_mark_complete(uint64_t session_id) {
    if (session_id == 0) {
        return -1;
    }

    pthread_mutex_lock(&s_mutex);

    SessionRecord *slot = find_slot_locked(session_id);
    if (!slot) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    slot->status = SESSION_COMPLETE;
    slot->last_activity = time(NULL);

    pthread_mutex_unlock(&s_mutex);
    return 0;
}

int session_validate_resume(uint64_t session_id,
                            const char *topic,
                            uint32_t fingerprint,
                            uint32_t proposed_file,
                            uint64_t proposed_offset,
                            uint32_t *out_auth_file,
                            uint64_t *out_auth_offset,
                            uint16_t *out_flags) {
    if (session_id == 0 || !topic || !out_auth_file || !out_auth_offset || !out_flags) {
        return -1;
    }

    pthread_mutex_lock(&s_mutex);

    SessionRecord *slot = find_slot_locked(session_id);
    if (!slot) {
        pthread_mutex_unlock(&s_mutex);
        return -2; /* Session not found */
    }

    /* Check TTL */
    time_t now = time(NULL);
    if (now - slot->last_activity > SESSION_DEFAULT_TTL) {
        slot->in_use = false;
        pthread_mutex_unlock(&s_mutex);
        return -3; /* Session expired */
    }

    /* Check if already complete */
    if (slot->status == SESSION_COMPLETE) {
        pthread_mutex_unlock(&s_mutex);
        return -4; /* Already complete */
    }

    /* Check topic match */
    if (strncmp(slot->topic, topic, sizeof(slot->topic)) != 0) {
        pthread_mutex_unlock(&s_mutex);
        return -5; /* Topic mismatch */
    }

    /* Check manifest fingerprint match */
    if (slot->manifest_fingerprint != fingerprint) {
        pthread_mutex_unlock(&s_mutex);
        return -6; /* Manifest changed */
    }

    /* Return server authoritative checkpoint */
    *out_auth_file = slot->current_file_index;
    *out_auth_offset = slot->current_file_offset;
    *out_flags = FLAG_NONE;

    /* If client proposed progress beyond server's authoritative checkpoint, force rewind */
    if (proposed_file > slot->current_file_index ||
        (proposed_file == slot->current_file_index && proposed_offset > slot->current_file_offset)) {
        *out_flags |= FLAG_CONFLICT;
    }

    slot->last_activity = now;

    pthread_mutex_unlock(&s_mutex);
    return 0;
}

int session_cleanup_stale(int ttl_seconds) {
    if (ttl_seconds <= 0) {
        ttl_seconds = SESSION_DEFAULT_TTL;
    }

    pthread_mutex_lock(&s_mutex);

    time_t now = time(NULL);
    int cleaned = 0;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (s_table[i].in_use && (now - s_table[i].last_activity >= ttl_seconds)) {
            s_table[i].in_use = false;
            cleaned++;
        }
    }

    pthread_mutex_unlock(&s_mutex);
    return cleaned;
}
