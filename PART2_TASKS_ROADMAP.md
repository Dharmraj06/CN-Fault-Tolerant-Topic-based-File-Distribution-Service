# IT305 Computer Networks Project: Part 2 Phase-Wise Execution Roadmap
**Project:** Fault-Tolerant Topic-Based File Distribution Service  
**Academic Group:** Merged Group 071 + Group 072 (DA-IICT)  
**Execution Model:** 100% Independent Phase-Wise Ownership (Zero Inter-Member Overlap)  
**Final Submission Deadline:** 15 October 2026 (23:59 IST)

---

## 1. Executive Summary & Non-Overlapping Ownership Model

To eliminate inter-day coordination bottlenecks, file conflicts, and work overlap, the entire Part 2 project is divided into **six independent phases**. 

### Core Rules for Independent Execution:
1. **Single Owner Per Phase:** Each phase is assigned to **exactly one member**. No two members will edit the same file or work within the same phase.
2. **Strict Interface Freezing:** All inter-phase communication relies strictly on C header contracts (`.h` files) defined in Phase 1. Once headers are established, each member builds their respective module in complete isolation.
3. **Autonomous Compilability:** Every implementation directory (`Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`) has its own independent `Makefile` and can be built and tested without waiting on unfinished experimental code from other phases.

### Master Phase Allocation Table

| Phase | Phase Title | Sole Responsible Member | Student ID | Exclusive Files Owned | Key Deliverable |
|:---:|---|---|:---:|---|---|
| **Phase 1** | Wire Protocol Framing & Topic Manifest Engine | **Jay Vaghela** | 202401237 | `common/protocol.h`<br>`common/protocol.c`<br>`common/topic_manager.h`<br>`common/topic_manager.c`<br>`Part1/server.c`<br>`Part1/Makefile` | Binary framing engine, topic scanner, frozen headers, and Part 1 baseline server. |
| **Phase 2** | Fault Injection Engine & Part 2 Case 1 End-to-End | **Atharv Trivedi** | 202401230 | `common/fault_injection.h`<br>`common/fault_injection.c`<br>`Part2_Case1/server.c`<br>`Part2_Case1/client.c`<br>`Part2_Case1/Makefile` | Fault injection mechanism, Case 1 fault-injecting server, and Case 1 restart client. |
| **Phase 3** | In-Memory Session Management & Part 2 Case 2 Server | **Dharmraj Vaghela** | 202401236 | `common/session.h`<br>`common/session.c`<br>`Part2_Case2/server.c`<br>`Part2_Case2/Makefile` | Thread-safe session table, checkpoint commit engine, and Case 2 resume server. |
| **Phase 4** | Bounded File Streaming & Part 2 Case 2 Client | **Vivek Dodiya** | 202401253 | `common/file_transfer.h`<br>`common/file_transfer.c`<br>`Part1/client.c`<br>`Part2_Case2/client.c` | Bounded file I/O, per-chunk ACK tracking, and Case 2 reconnecting resume client. |
| **Phase 5** | Parallel TCP Architecture & Part 2 Case 2 Enhanced | **Shlok Topaliya** | 202401229 | `common/work_plan.h`<br>`common/work_plan.c`<br>`Part2_Case2_Enhanced/server.c`<br>`Part2_Case2_Enhanced/client.c`<br>`Part2_Case2_Enhanced/Makefile` | Work unit partitioning, multi-socket worker server, and concurrent `pwrite` client. |
| **Phase 6** | Logging, Test Suite, Benchmarking & Submission Package | **Yug Malaviya** | 202401256 | `common/logging.h`<br>`common/logging.c`<br>`scripts/test_correctness.sh`<br>`scripts/run_benchmarks.py`<br>`scripts/plot_results.py`<br>`scripts/package_submission.sh`<br>`Results/*`<br>`FinalReport_Group_071_072.pdf`<br>`README.md` | Thread-safe logger, automated test harness, benchmark runner, plots, and final report PDF. |

---

## 2. Detailed Phase Specifications

```
  ┌──────────────────────────────────────────────────────────────┐
  │         PHASE 1: Wire Framing & Topic Manifest Engine        │
  │                     Owner: Jay Vaghela                       │
  └──────────────────────────────┬───────────────────────────────┘
                                 │ Frozen Header Contracts
         ┌───────────────────────┼────────────────────────┐
         ▼                       ▼                        ▼
  ┌──────────────┐       ┌──────────────┐         ┌──────────────┐
  │   PHASE 2    │       │   PHASE 3    │         │   PHASE 4    │
  │ Fault & Case1│       │ Session & Srv│         │ FileIO & Clnt│
  │Atharv Trivedi│       │Dharmraj Vagh.│         │ Vivek Dodiya │
  └──────────────┘       └───────┬──────┘         └───────┬──────┘
                                 └───────────┬────────────┘
                                             ▼
                                 ┌───────────────────────┐
                                 │        PHASE 5        │
                                 │ Case 2 Enhanced (TCP) │
                                 │    Shlok Topaliya     │
                                 └───────────┬───────────┘
                                             ▼
                                 ┌───────────────────────┐
                                 │        PHASE 6        │
                                 │ Testing, Plots, Report│
                                 │     Yug Malaviya      │
                                 └───────────────────────┘
```

---

### Phase 1: Wire Protocol Framing & Topic Manifest Engine
- **Sole Owner:** **Jay Vaghela** (Student ID: 202401237)
- **Primary Responsibility:** Protocol wire framing, integer endianness conversions, topic directory traversal, and Part 1 baseline server validation.
- **Prerequisites:** None (Foundation phase).
- **Exclusive Files Owned:**
  - `common/protocol.h`
  - `common/protocol.c`
  - `common/topic_manager.h`
  - `common/topic_manager.c`
  - `Part1/server.c`
  - `Part1/Makefile`

#### Specific Technical Tasks:
1. **16-Byte Binary Frame Engine (`common/protocol.*`):**
   - Implement the standard 16-byte fixed binary header structure:
     - `magic` (2 bytes, `0x5446` - 'TF')
     - `version` (1 byte, `0x01`)
     - `type` (1 byte, enum for `GET`, `TOPIC_OK`, `TOPIC_ERROR`, `FILE_BEGIN`, `FILE_DATA`, `FILE_END`, `TRANSFER_END`, `SESSION_START`, `SESSION_ACCEPT`, `CHUNK_ACK`, `SESSION_RESUME`, `RESUME_ACCEPT`, `WORK_PLAN`, `WORK_ASSIGN`, `WORK_DATA`, `WORK_END`, `ERROR`, `CLOSE`)
     - `flags` (2 bytes)
     - `reserved` (2 bytes)
     - `payload_len` (8 bytes, `uint64_t` converted using 64-bit host-to-network byte order)
   - Implement framing primitives:
     - `int send_all(int sockfd, const void *buf, size_t len)`: Loops until all bytes are sent, retrying on `EINTR`.
     - `int recv_all(int sockfd, void *buf, size_t len)`: Loops until exact length is filled, handling partial TCP reads and clean disconnections.
     - `int send_frame(int sockfd, uint8_t type, uint16_t flags, const void *payload, uint64_t payload_len)`.
     - `int recv_frame_header(int sockfd, FrameHeader *hdr)`.
     - `int recv_frame_payload(int sockfd, const FrameHeader *hdr, void *buf, size_t max_len)`.
2. **Topic Discovery & Security Engine (`common/topic_manager.*`):**
   - Implement topic path resolver preventing directory traversal (`..`, `/`, `\`, NUL characters).
   - Implement `build_topic_manifest(const char *root_dir, const char *topic, TopicManifest **out_manifest)`:
     - Scans topic folder using `opendir()` and `readdir()`.
     - Filters only regular files using `stat()` / `S_ISREG` (excluding subdirectories and symlinks).
     - Sorts filenames lexicographically via `qsort()` to guarantee identical manifest order across connections.
     - Computes manifest fingerprint (total file count + checksum of names/sizes).
3. **Part 1 Baseline Server (`Part1/server.c` & `Part1/Makefile`):**
   - Import and verify sequential and multi-threaded server implementation using the frozen protocol.
   - Verify server CLI compliance: `./server <port> <topics_root_dir>`.

#### Acceptance Criteria & Definition of Done:
- `common/protocol.h` and `common/topic_manager.h` compile with zero warnings under `gcc -Wall -Wextra -pedantic`.
- Loopback test verifies that arbitrary binary payloads (including zeros and image bytes) are framed and parsed without corruption.
- Attempted path traversal (e.g., `GET ../../etc/passwd`) is rejected with `TOPIC_ERROR`.

---

### Phase 2: Fault Injection Engine & Part 2 Case 1 End-to-End
- **Sole Owner:** **Atharv Trivedi** (Student ID: 202401230)
- **Primary Responsibility:** Probabilistic connection drop mechanism and complete Part 2 Case 1 implementation (server fault insertion and client full-topic restart).
- **Prerequisites:** Phase 1 header contracts (`protocol.h`, `topic_manager.h`).
- **Exclusive Files Owned:**
  - `common/fault_injection.h`
  - `common/fault_injection.c`
  - `Part2_Case1/server.c`
  - `Part2_Case1/client.c`
  - `Part2_Case1/Makefile`

#### Specific Technical Tasks:
1. **Fault Injection Subsystem (`common/fault_injection.*`):**
   - Implement `int init_fault_injection(double failure_probability, unsigned int seed)`.
   - Validate CLI `<failure probability>` argument strictly in range `[0.0, 1.0]` (reject non-numeric, negative, or `> 1.0` inputs).
   - Implement `int should_inject_fault(void)`:
     - Uses `rand_r()` or seeded PRNG to evaluate probability before each `FILE_DATA` chunk.
   - Implement `void execute_fault_disconnect(int client_sock)`:
     - Logs the intentional disconnection event (topic, file, offset).
     - Closes the active socket immediately without sending termination frames.
2. **Part 2 Case 1 Server (`Part2_Case1/server.c`):**
   - Multi-threaded server creating one detached worker thread per client connection.
   - CLI invocation: `./server <port> <topics_root_dir> <failure probability>`.
   - Before streaming each chunk of file data, calls `should_inject_fault()`. If triggered, executes abrupt disconnect and terminates worker thread.
   - Server retains **zero session state**; every incoming connection is treated as an independent topic transfer.
3. **Part 2 Case 1 Client (`Part2_Case1/client.c`):**
   - CLI invocation: `./client <server_ip> <server_port> <topic> <output_dir>`.
   - Detects connection drops (`recv_all` returns `<= 0` before `TRANSFER_END`).
   - On failure:
     - Closes broken socket descriptor.
     - **Purges all incomplete downloaded data:** deletes partially written files or the entire `<output_dir>/<topic>/` directory.
     - Pauses for a bounded retry delay.
     - Reconnects to server and re-issues `GET <topic>`.
     - Restarts topic transfer entirely from file 0, offset 0.
   - Implements bounded maximum retry count (e.g., 30 attempts) to prevent infinite loops when failure probability is 1.0.

#### Acceptance Criteria & Definition of Done:
- Running `make` in `Part2_Case1/` produces `server` and `client`.
- When tested with `failure_probability = 0.05`, server drops connections mid-transfer; client detects disconnect, cleans destination directory, reconnects, and eventually completes all files without leaving truncated files.
- Byte-level comparison (`diff -r`) between server source directory and client output directory matches 100%.

---

### Phase 3: In-Memory Session Management & Part 2 Case 2 Server
- **Sole Owner:** **Dharmraj Vaghela** (Student ID: 202401236)
- **Primary Responsibility:** Server-side thread-safe session table, checkpoint commit state machine, and Case 2 server offset-resumption protocol.
- **Prerequisites:** Phase 1 header contracts (`protocol.h`, `topic_manager.h`) and Phase 2 (`fault_injection.h`).
- **Exclusive Files Owned:**
  - `common/session.h`
  - `common/session.c`
  - `Part2_Case2/server.c`
  - `Part2_Case2/Makefile`

#### Specific Technical Tasks:
1. **Thread-Safe In-Memory Session Engine (`common/session.*`):**
   - Define session data structure:
     ```c
     typedef struct {
         uint64_t session_id;
         char topic[128];
         uint32_t manifest_fingerprint;
         uint32_t current_file_index;
         char current_filename[256];
         uint64_t current_file_offset;
         uint32_t completed_files_count;
         enum { SESSION_ACTIVE, SESSION_INTERRUPTED, SESSION_COMPLETE, SESSION_EXPIRED } status;
         time_t last_checkpoint_time;
         time_t last_activity_time;
         uint64_t bytes_useful;
         uint64_t bytes_redundant;
     } SessionState;
     ```
   - Protect global session table with POSIX `pthread_mutex_t`.
   - Implement core state routines:
     - `SessionState* session_create(const char *topic, uint32_t manifest_fingerprint)`: Generates cryptographically safe 64-bit random `session_id`.
     - `SessionState* session_lookup(uint64_t session_id)`.
     - `int session_commit_chunk(uint64_t session_id, uint32_t file_index, uint64_t end_offset)`: Atomically commits received checkpoint upon `CHUNK_ACK`.
     - `int session_validate_resume(uint64_t session_id, const char *topic, uint32_t *out_file_idx, uint64_t *out_offset)`: Validates client resume proposal against server authority.
     - `void session_cleanup_stale(int ttl_seconds)`.
   - **Concurrency Rule:** Never hold session mutex while doing socket `send()`/`recv()` or disk I/O.
2. **Part 2 Case 2 Server (`Part2_Case2/server.c`):**
   - CLI invocation: `./server <port> <topics_root_dir> <failure probability>`.
   - Accepts client connections in multi-threaded listener loop.
   - On `SESSION_START`: creates new session, returns `SESSION_ACCEPT` containing assigned `session_id` and file count.
   - On `SESSION_RESUME`: invokes `session_validate_resume()`, sends `RESUME_ACCEPT` containing authoritative `file_index` and `offset`.
   - Streams files starting from authoritative checkpoint:
     - Uses `lseek()` to start file read at committed `current_file_offset`.
     - After each sent chunk, listens for `CHUNK_ACK` from client.
     - Commits checkpoint via `session_commit_chunk()`.
     - Evaluates fault injection before each chunk; if triggered, abruptly drops socket and marks session `SESSION_INTERRUPTED`.

#### Acceptance Criteria & Definition of Done:
- Compiles cleanly in `Part2_Case2/` producing `server`.
- Reconnecting clients with valid `session_id` are recognized; server resumes from exact committed byte offset without resending already-acknowledged chunks or prior completed files.
- Thread concurrency tests demonstrate that 10 concurrent worker threads can update and query sessions without deadlocks or race conditions.

---

### Phase 4: Bounded File Streaming & Part 2 Case 2 Client
- **Sole Owner:** **Vivek Dodiya** (Student ID: 202401253)
- **Primary Responsibility:** Chunked file I/O operations, client-side checkpoint persistence, `CHUNK_ACK` transmission, and destination file seeking/truncation on resume.
- **Prerequisites:** Phase 1 header contracts (`protocol.h`) and Phase 3 (`session.h`).
- **Exclusive Files Owned:**
  - `common/file_transfer.h`
  - `common/file_transfer.c`
  - `Part1/client.c`
  - `Part2_Case2/client.c`

#### Specific Technical Tasks:
1. **Bounded File I/O Engine (`common/file_transfer.*`):**
   - Implement chunked reading and writing helpers with configurable bounded buffer (e.g., 64 KB chunk size):
     - `int open_file_read(const char *path, uint64_t start_offset, int *fd)`.
     - `int write_chunk_to_file(int fd, uint64_t offset, const void *data, size_t size)`.
     - `int truncate_file_at(const char *filepath, uint64_t length)`.
   - Never load entire files into RAM memory; stream using bounded stack/heap buffers.
2. **Part 1 Baseline Client (`Part1/client.c`):**
   - Import and verify standard Part 1 client: connects, issues `GET <topic>`, creates `<output_dir>/<topic>/`, writes files sequentially, and exits cleanly on `TRANSFER_END`.
3. **Part 2 Case 2 Client (`Part2_Case2/client.c`):**
   - CLI invocation: `./client <server_ip> <server_port> <topic> <output_dir>`.
   - Session Lifecycle:
     - First connection: sends `SESSION_START`, records `session_id` returned in `SESSION_ACCEPT`.
     - Creates local destination directory `<output_dir>/<topic>/`.
     - Receives `FILE_BEGIN`, opens destination file.
     - On each received `FILE_DATA` chunk:
       - Writes chunk to disk at current offset.
       - Flushes bytes and sends `CHUNK_ACK` with `session_id`, `file_index`, and new `end_offset`.
       - Updates local checkpoint tracking.
     - On receiving `FILE_END`: closes destination file, marks file fully committed.
   - Reconnection & Resume:
     - On connection failure: detects socket error, closes socket, pauses with backoff.
     - Connects anew, sends `SESSION_RESUME` with stored `session_id` and local checkpoint.
     - Receives `RESUME_ACCEPT` containing server's authoritative `file_index` and `offset`.
     - **Offset Synchronization:** If local file has unacknowledged trailing bytes, uses `ftruncate()` to align to server authoritative offset; seeks file pointer with `lseek()`.
     - Resumes receiving remaining data chunks until `TRANSFER_END`.

#### Acceptance Criteria & Definition of Done:
- Running `make` in `Part2_Case2/` produces verified `client`.
- When tested against Case 2 server under 10% fault injection, client resumes smoothly from mid-file offsets without creating corrupt or duplicated data chunks.
- Output directory contains bit-exact replicas of topic images verified via SHA256 checksums.

---

### Phase 5: Parallel TCP Architecture & Part 2 Case 2 Enhanced
- **Sole Owner:** **Shlok Topaliya** (Student ID: 202401229)
- **Primary Responsibility:** Topic work-unit partitioning, parallel TCP connection pooling, thread-safe concurrent file writing via `pwrite()`, and independent unit recovery.
- **Prerequisites:** Phase 1 (`protocol.h`, `topic_manager.h`), Phase 3 (`session.h`), and Phase 4 (`file_transfer.h`).
- **Exclusive Files Owned:**
  - `common/work_plan.h`
  - `common/work_plan.c`
  - `Part2_Case2_Enhanced/server.c`
  - `Part2_Case2_Enhanced/client.c`
  - `Part2_Case2_Enhanced/Makefile`

#### Specific Technical Tasks:
1. **Work Partitioning Engine (`common/work_plan.*`):**
   - Define work-unit data structures:
     ```c
     typedef struct {
         uint32_t unit_id;
         uint32_t file_index;
         uint64_t start_offset;
         uint64_t end_offset; // exclusive boundary
         uint64_t bytes_transferred;
         enum { UNIT_IDLE, UNIT_IN_PROGRESS, UNIT_COMMITTED, UNIT_FAILED } state;
     } WorkUnit;
     ```
   - Implement `int create_work_plan(TopicManifest *manifest, int num_connections, WorkPlan **out_plan)`:
     - Partition policy: If `file_count >= num_connections`, assign discrete files to separate connections. If `file_count < num_connections`, partition large files into non-overlapping chunk ranges.
2. **Enhanced Server (`Part2_Case2_Enhanced/server.c`):**
   - CLI invocation: `./server <port> <topics_root_dir> <failure probability>`.
   - Supports multiple concurrent TCP connections belonging to the same `session_id`.
   - Dispatches distinct `WorkUnit` allocations across available client sockets (`WORK_ASSIGN`).
   - Receives per-unit chunk ACKs; if an individual socket experiences an injected fault, only that specific `WorkUnit` is flagged `UNIT_FAILED` and made available for reassignment. Other active connections continue uninterrupted.
3. **Enhanced Client Coordinator (`Part2_Case2_Enhanced/client.c`):**
   - CLI invocation: `./client <server_ip> <server_port> <topic> <output_dir> [num_connections]`.
   - **Default Behavior:** If `num_connections` is omitted, defaults strictly to `1`, ensuring 100% compatibility with mandatory grading scripts.
   - Spawns worker threads (e.g., 2 to 4 parallel TCP connections) targeting the server.
   - **Concurrent Write Safety:** Opens destination files and writes incoming chunks using `pwrite(fd, buf, count, offset)`. Because byte ranges are non-overlapping, multiple threads write concurrently without file corruptions or lock contention.
   - A single connection drop triggers reconnection only for that worker thread's assigned `WorkUnit`, preserving progress across all other parallel streams.

#### Acceptance Criteria & Definition of Done:
- Running `make` in `Part2_Case2_Enhanced/` builds `server` and `client`.
- Mandatory 4-argument client invocation runs cleanly without errors.
- Running with 4 parallel connections on multi-file topics demonstrates higher throughput than single-connection Case 2 on low-latency networks while maintaining 100% file integrity.

---

### Phase 6: Logging, Automated Test Suite, Benchmarking & Submission Package
- **Sole Owner:** **Yug Malaviya** (Student ID: 202401256)
- **Primary Responsibility:** Thread-safe high-precision logging, automated regression and edge-case testing, experimental data collection, graph plotting, and final submission archive generation.
- **Prerequisites:** All prior phases operational.
- **Exclusive Files Owned:**
  - `common/logging.h`
  - `common/logging.c`
  - `scripts/test_correctness.sh`
  - `scripts/run_benchmarks.py`
  - `scripts/plot_results.py`
  - `scripts/package_submission.sh`
  - `Results/*` (CSV logs and generated plots)
  - `FinalReport_Group_071_072.pdf`
  - `README.md`

#### Specific Technical Tasks:
1. **Thread-Safe Logging Module (`common/logging.*`):**
   - Implement thread-safe logging functions with formatted timestamps, socket IDs, topics, event descriptions, and byte counts.
   - Record metrics: total socket bytes sent, useful payload bytes delivered, redundant bytes retransmitted, and monotonic transfer elapsed time (`clock_gettime(CLOCK_MONOTONIC)`).
2. **Automated Correctness & Edge-Case Test Suite (`scripts/test_correctness.sh`):**
   - Validate CLI argument checks: missing arguments, non-numeric ports, invalid failure probabilities (`-0.5`, `1.5`, `abc`).
   - Validate transfers across diverse topics: empty folders, single large files, multiple small files, binary images (`animals10`).
   - Verify that output files are placed strictly in `<output_dir>/<topic>/`.
   - Perform automated `sha256sum` comparison on every received file against source directory.
3. **Benchmark Execution Suite (`scripts/run_benchmarks.py`):**
   - Automatically orchestrate server and client instances across the prescribed test matrix:
     - **Part 1:** Sequential vs. Multi-Threaded server (1, 2, 4, 8 concurrent clients).
     - **Part 2 Case 1 vs. Case 2:** Varying fault rates: `0%`, `1%`, `5%`, `10%`.
     - **Case 2 vs. Case 2 Enhanced:** Parallel connection counts: 1, 2, 4 connections.
   - Record real, measured values into structured CSV files under `Results/`.
4. **Publication-Quality Graph Generation (`scripts/plot_results.py`):**
   - Use Python (`matplotlib`) to parse CSV files and generate required plots:
     - `Part1_throughput_vs_clients.png`
     - `Part2_Case1_vs_Case2_throughput.png`
     - `Part2_Case1_vs_Case2_overhead.png`
     - `Part2_Enhanced_speedup_comparison.png`
5. **Final Documentation & Packaging (`scripts/package_submission.sh`):**
   - Write comprehensive root `README.md` with build instructions, CLI invocations, and fault injection details.
   - Compile consolidated `FinalReport_Group_071_072.pdf` including:
     - Group cover page (member names, IDs, one-line contribution statement).
     - Implementation architecture & design deviations.
     - Authentic measurement tables and analysis graphs.
   - Automated packaging script to create `FinalSubmission_Group_071_072.tar.gz` matching the exact directory layout required by the assignment.

#### Acceptance Criteria & Definition of Done:
- All automated test scripts execute and pass with exit code `0`.
- Real, un-fabricated benchmark CSV logs and graphs exist in `Results/`.
- Submission tarball contains no temporary object files (`.o`), hidden `.git` folders, or core dumps, and compiles cleanly upon fresh extraction.

---

## 3. Inter-Phase Dependency & Interface Freezing Strategy

To guarantee zero inter-member code conflicts, the interface contracts are structured as follows:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   PHASE 1: PROTOCOL HEADER CONTRACT                    │
│                        `common/protocol.h`                             │
├────────────────────────────────────────────────────────────────────────┤
│ • FrameHeader (16 bytes fixed, big-endian uint64 length)               │
│ • Message Types: GET, OK, FILE_BEGIN, FILE_DATA, FILE_END, etc.        │
│ • Helpers: send_all(), recv_all(), send_frame(), recv_frame()          │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Included by
          ┌─────────────────────────┼────────────────────────┐
          ▼                         ▼                        ▼
┌──────────────────┐      ┌──────────────────┐     ┌──────────────────┐
│     PHASE 2      │      │     PHASE 3      │     │     PHASE 4      │
│ `fault_injection`│      │ `common/session` │     │ `file_transfer`  │
│  - Probability   │      │  - SessionState  │     │  - Bounded read  │
│  - Socket drop   │      │  - Commit ACK    │     │  - Chunk write   │
│                  │      │  - Resume check  │     │  - ftruncate     │
└──────────────────┘      └─────────┬────────┘     └─────────┬────────┘
                                    │                        │
                                    └───────────┬────────────┘
                                                ▼
                                  ┌───────────────────────────┐
                                  │          PHASE 5          │
                                  │    `common/work_plan`     │
                                  │     - WorkUnit array      │
                                  │     - Range partitioning  │
                                  └─────────────┬─────────────┘
                                                ▼
                                  ┌───────────────────────────┐
                                  │          PHASE 6          │
                                  │      `common/logging`     │
                                  │     - Monotonic timing    │
                                  │     - Benchmark scripts   │
                                  └───────────────────────────┘
```

1. **Header Boundary Rule:** Only the designated owner of each module is permitted to commit changes to its `.h` and `.c` files.
2. **Mocking During Parallel Development:** Vivek (Phase 4 Client) can test against Phase 1 headers and mock session responses while Dharmraj (Phase 3 Server) completes session table logic.
3. **No Cross-Folder Pollution:** Source files in `Part2_Case1/`, `Part2_Case2/`, and `Part2_Case2_Enhanced/` do not share mutable globals or cross-link across case folders.

---

## 4. Final Submission Checklist

- [ ] **Directory Layout:** Required folders exist: `Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`, and `Results/`.
- [ ] **Clean Compilation:** Running `make` inside every case folder generates `server` and `client` binaries with zero warnings.
- [ ] **Mandatory Server CLI:**
  - Part 1: `./server <port> <topics_root_dir>`
  - Part 2 (all cases): `./server <port> <topics_root_dir> <failure probability>`
- [ ] **Mandatory Client CLI:**
  - All cases: `./client <server_ip> <server_port> <topic> <output_dir>` (Enhanced defaults to 1 connection when 4th argument is omitted).
- [ ] **Output Layout:** Downloaded files are placed in `<output_dir>/<topic>/`.
- [ ] **Data Integrity:** Byte-for-byte SHA256 checksum match between server topic source and client received files.
- [ ] **Real Metrics:** Authentic CSV logs and generated throughput/overhead plots present in `Results/`.
- [ ] **Report & Documentation:** `FinalReport_Group_071_072.pdf` and `README.md` fully completed and placed in root directory.
- [ ] **Archive Validation:** `FinalSubmission_Group_071_072.tar.gz` extracts and builds cleanly on a fresh Linux environment.
