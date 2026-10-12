# ARCHITECTURE & INTERFACES SPECIFICATION
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Document**: Architectural Design, Modular Contracts & C Interface Headers  
**Target Platform**: Linux / POSIX C (C11/C99, `gcc`, `-Wall -Wextra -pthread`)  

---

## 1. System Architecture & Directory Layout

To satisfy the strict automated grading criteria, the project maintains four independent deliverable folders. Each folder contains its own self-contained `server.c`, `client.c`, helper modules, and `Makefile`:

```
CN-Fault-Tolerant-Topic-based-File-Distribution-Service/
├── common/                  <-- Single source of truth during development
│   ├── protocol.h / .c      <-- Framing, serialization, send_all/recv_all
│   ├── topic_manager.h / .c <-- Topic scanning, manifests, path security
│   ├── file_transfer.h / .c <-- Chunked file I/O, pwrite, integrity
│   ├── fault_injection.h/.c <-- Probabilistic connection interruption
│   ├── session.h / .c       <-- In-memory session table & checkpointing
│   ├── logging.h / .c       <-- Thread-safe logging & CSV metrics
│   └── work_plan.h / .c     <-- Case 2 Enhanced parallel work partitioning
│
├── Part1/                   <-- Deliverable 1: Basic Sequential & Multi-Threaded
│   ├── server.c             <-- Supports single-threaded or multi-threaded via config
│   ├── client.c             <-- Standard sequential client
│   ├── Makefile             <-- Builds ./server and ./client
│   └── [linked/copied common modules: protocol, topic_manager, file_transfer, logging]
│
├── Part2_Case1/             <-- Deliverable 2: Fault Injection + Full Restart
│   ├── server.c             <-- Multi-threaded server + fault injection
│   ├── client.c             <-- Client with abrupt drop detection & full restart loop
│   ├── Makefile             <-- Builds ./server and ./client
│   └── [common modules + fault_injection]
│
├── Part2_Case2/             <-- Deliverable 3: Session Management + Checkpoint Resume
│   ├── server.c             <-- Multi-threaded server + session table + checkpoint validation
│   ├── client.c             <-- Client with local checkpointing & resume handshake
│   ├── Makefile             <-- Builds ./server and ./client
│   └── [common modules + fault_injection + session]
│
├── Part2_Case2_Enhanced/    <-- Deliverable 4: Parallel TCP Connections
│   ├── server.c             <-- Parallel connection coordinator + work unit dispatcher
│   ├── client.c             <-- Multi-connection client with range pwrite & barrier
│   ├── Makefile             <-- Builds ./server and ./client
│   └── [common modules + fault_injection + session + work_plan]
│
├── Results/                 <-- Deliverable 5: Raw CSV logs and plotted graphs
├── scripts/                 <-- Test harnesses, benchmark automation, plotting scripts
└── docs/roadmap/            <-- Project documentation & execution plans
```

---

## 2. Core Shared Module Headers

The following headers define the immutable API contracts between modules. Once implemented and approved, these contracts allow all six members to develop their respective components simultaneously without breaking integration.

---

### 2.1 `protocol.h` — Framing & Wire Serialization
**Owner**: Jay Vaghela (202401237)  
**Dependents**: All servers and clients.

```c
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define PROTOCOL_MAGIC        0x5446  /* 'TF' in ASCII */
#define PROTOCOL_VERSION      1
#define MAX_TOPIC_NAME        128
#define MAX_FILENAME          256
#define DEFAULT_CHUNK_SIZE    16384   /* 16 KB chunk */
#define MAX_FRAME_PAYLOAD     (2 * 1024 * 1024) /* 2 MB safety limit */

/* 16-byte fixed binary frame header */
#pragma pack(push, 1)
typedef struct {
    uint16_t magic;         /* 0x5446 */
    uint8_t  version;       /* 1 */
    uint8_t  type;          /* FrameType */
    uint16_t flags;         /* Bitmask */
    uint16_t sequence_id;   /* Transaction ID */
    uint64_t payload_len;   /* Exact payload byte count */
} FrameHeader;
#pragma pack(pop)

/* Core Socket Helper Routines */
ssize_t send_all(int sockfd, const void *buf, size_t count);
ssize_t recv_all(int sockfd, void *buf, size_t count);

/* High-Level Frame Send & Receive */
int send_frame(int sockfd, uint8_t type, uint16_t flags, uint16_t seq_id,
               const void *payload, uint64_t payload_len);
int recv_frame_header(int sockfd, FrameHeader *hdr);
int recv_frame_payload(int sockfd, const FrameHeader *hdr, void *payload_buf, size_t max_buf_size);

/* 64-bit Network Byte Order Helpers */
uint64_t hton64(uint64_t val);
uint64_t ntoh64(uint64_t val);

#endif /* PROTOCOL_H */
```

---

### 2.2 `topic_manager.h` — Directory Scanning & Manifest Security
**Owner**: Jay Vaghela (202401237)  
**Dependents**: All servers.

```c
#ifndef TOPIC_MANAGER_H
#define TOPIC_MANAGER_H

#include <stdint.h>
#include "protocol.h"

#define MAX_FILES_PER_TOPIC 1024

typedef struct {
    char     filename[MAX_FILENAME];
    char     relative_path[MAX_FILENAME * 2];
    uint64_t file_size;
} FileEntry;

typedef struct {
    char      topic_name[MAX_TOPIC_NAME];
    char      resolved_dir[512];
    uint32_t  file_count;
    uint64_t  total_bytes;
    uint32_t  manifest_fingerprint;
    FileEntry files[MAX_FILES_PER_TOPIC];
} TopicManifest;

/* Validates that topic contains no '/', '\', '..', or hidden characters */
int validate_topic_name(const char *topic);

/* Scans <topics_root_dir>/<topic>, sorts regular files lexicographically, builds manifest */
int build_topic_manifest(const char *topics_root_dir, const char *topic, TopicManifest *manifest);

/* Calculates stable CRC32 fingerprint over ordered filenames and sizes */
uint32_t calculate_manifest_fingerprint(const TopicManifest *manifest);

#endif /* TOPIC_MANAGER_H */
```

---

### 2.3 `file_transfer.h` — Bounded Streaming & Disk I/O
**Owner**: Vivek Dodiya (202401253)  
**Dependents**: All servers and clients.

```c
#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <stdint.h>
#include <sys/types.h>

/* Ensures <output_dir>/<topic>/ directory exists on disk */
int ensure_directory_exists(const char *path);

/* Sanitizes received filename to prevent path traversal on client disk */
int sanitize_filename(const char *input_name, char *output_name, size_t max_len);

/* Streams a file over a socket from start_offset to end_offset in bounded chunks */
int stream_file_range(int sockfd, const char *filepath, uint32_t file_index,
                      uint64_t start_offset, uint64_t end_offset,
                      size_t chunk_size, uint64_t *bytes_sent,
                      int (*fault_hook)(int sockfd, void *user_data), void *user_data);

/* Writes a received chunk at an exact file offset (uses pwrite for thread safety) */
int write_chunk_to_file(const char *filepath, uint64_t offset, const void *data, size_t len);

/* Validates file size on disk against manifest */
int verify_file_size(const char *filepath, uint64_t expected_size);

#endif /* FILE_TRANSFER_H */
```

---

### 2.4 `fault_injection.h` — Probabilistic Disconnection Engine
**Owner**: Atharv Trivedi (202401230)  
**Dependents**: Part 2 Case 1, Case 2, Case 2 Enhanced servers.

```c
#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#include <stdbool.h>

typedef struct {
    double   probability;       /* Inclusive [0.0, 1.0] */
    unsigned int random_seed;   /* Seed for reproducibility */
    uint64_t decisions_made;
    uint64_t faults_injected;
} FaultInjector;

/* Initialize fault injector with validated probability [0.0, 1.0] */
int fault_injector_init(FaultInjector *fi, double prob, unsigned int seed);

/* Evaluates whether the current chunk transfer should be aborted */
bool should_inject_fault(FaultInjector *fi);

/* Abruptly interrupts a socket connection (clean shutdown + close) */
void execute_socket_interruption(int sockfd);

#endif /* FAULT_INJECTION_H */
```

---

### 2.5 `session.h` — In-Memory Session & Checkpoint Table
**Owner**: Dharmraj Vaghela (202401236)  
**Dependents**: Part 2 Case 2 and Case 2 Enhanced servers.

```c
#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include "protocol.h"

#define MAX_SESSIONS 256
#define SESSION_TTL_SECONDS 300 /* 5 minute expiration */

typedef enum {
    SESSION_STATE_UNUSED      = 0,
    SESSION_STATE_ACTIVE      = 1,
    SESSION_STATE_INTERRUPTED = 2,
    SESSION_STATE_COMPLETED   = 3,
    SESSION_STATE_EXPIRED     = 4
} SessionStatus;

typedef struct {
    uint64_t      session_id;
    SessionStatus status;
    char          topic[MAX_TOPIC_NAME];
    uint32_t      manifest_fingerprint;
    uint32_t      current_file_index;
    uint64_t      committed_file_offset;
    uint32_t      completed_files_count;
    time_t        created_at;
    time_t        last_activity;
    uint64_t      useful_bytes_transferred;
    uint64_t      redundant_bytes_transferred;
} SessionEntry;

typedef struct {
    SessionEntry    entries[MAX_SESSIONS];
    pthread_mutex_t lock;
} SessionTable;

/* Global session table lifecycle */
int session_table_init(SessionTable *table);
void session_table_destroy(SessionTable *table);

/* Allocate a new session ID for a topic request (thread-safe) */
uint64_t session_create(SessionTable *table, const char *topic, uint32_t fingerprint);

/* Validate a resume request from a reconnecting client (thread-safe) */
int session_validate_resume(SessionTable *table, uint64_t session_id,
                            const char *topic, uint32_t fingerprint,
                            uint32_t *resume_file_index, uint64_t *resume_offset);

/* Atomically commit checkpoint progress upon receiving client CHUNK_ACK */
int session_commit_checkpoint(SessionTable *table, uint64_t session_id,
                              uint32_t file_index, uint64_t end_offset);

/* Mark session as completed */
int session_mark_complete(SessionTable *table, uint64_t session_id);

/* Garbage collection for expired sessions */
int session_expire_stale(SessionTable *table, time_t current_time);

#endif /* SESSION_H */
```

---

### 2.6 `logging.h` — Thread-Safe Logging & CSV Statistics
**Owner**: Shlok Topaliya (202401229)  
**Dependents**: All servers and clients.

```c
#ifndef LOGGING_H
#define LOGGING_H

#include <stdint.h>
#include <time.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3
} LogLevel;

/* High-precision monotonic timer */
double get_monotonic_time_sec(void);

/* Thread-safe stdout/file logging */
void log_msg(LogLevel level, const char *fmt, ...);

/* Records one CSV line for benchmark data */
void record_csv_result(const char *csv_filename, const char *case_name,
                       int client_id, const char *topic, int concurrency,
                       double failure_prob, int parallel_conns,
                       uint64_t useful_bytes, uint64_t total_socket_bytes,
                       double elapsed_seconds, int retries);

#endif /* LOGGING_H */
```

---

### 2.7 `work_plan.h` — Parallel Work Partitioning (Case 2 Enhanced)
**Owner**: Shlok Topaliya (202401229)  
**Dependents**: Part 2 Case 2 Enhanced server and client.

```c
#ifndef WORK_PLAN_H
#define WORK_PLAN_H

#include <stdint.h>
#include "topic_manager.h"

#define MAX_WORK_UNITS 1024

typedef struct {
    uint32_t unit_id;
    uint32_t file_index;
    uint64_t start_offset;
    uint64_t end_offset;    /* Exclusive byte offset */
    int      is_completed;
    int      assigned_worker;
} WorkUnit;

typedef struct {
    uint64_t session_id;
    uint32_t unit_count;
    WorkUnit units[MAX_WORK_UNITS];
} WorkPlan;

/* Partitions manifest into work units across N parallel connections */
int build_work_plan(const TopicManifest *manifest, uint64_t session_id,
                    int requested_connections, WorkPlan *plan);

/* Retrieves next available incomplete work unit for a worker connection */
int get_next_work_unit(WorkPlan *plan, int worker_id, WorkUnit *out_unit);

/* Marks work unit completed */
int complete_work_unit(WorkPlan *plan, uint32_t unit_id);

#endif /* WORK_PLAN_H */
```

---

## 3. Memory Ownership & Thread-Safety Rules

1. **Heap Allocation Discipline**:
   - Every `malloc()` or `calloc()` must have an immediate, explicit `free()` path.
   - For thread workers, worker arguments (`WorkerArgs *args`) are heap-allocated by the listener thread before `pthread_create()`, and freed by the worker thread immediately before `pthread_exit()`.
2. **Session Table Synchronization**:
   - `SessionTable.lock` protects table lookups, insertions, checkpoint commits, and expiration.
   - **Critical Rule**: A worker thread must *never* perform `send()`, `recv()`, or disk file I/O while holding `SessionTable.lock`. State must be copied to stack variables under the mutex, the mutex released, and then I/O performed.
3. **Signal Safety**:
   - All server and client binaries must invoke `signal(SIGPIPE, SIG_IGN)` at startup to prevent abrupt process termination when writing to a dropped TCP connection.
