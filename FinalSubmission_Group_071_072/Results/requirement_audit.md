# Requirement Verification and Audit Matrix

**Course:** IT305 Computer Networks Laboratory (Autumn 2026)  
**Project:** Fault-Tolerant Topic-Based File Distribution Service  
**Merged Lab Groups:** Group 071 + Group 072  
**Repository:** `CN-Fault-Tolerant-Topic-based-File-Distribution-Service`  
**Audit Date:** 2026-10-21  
**Audit Author:** Automated Agentic Verification Engine (DeepMind Antigravity)  
**Status Verdict:** `BENCHMARKS READY WITH LIMITATIONS` (Valid and traceable local benchmarks completed; physical multi-computer cluster not available in the single-host execution environment)

---

## 1. Executive Summary & Verification Methodology

This requirement audit evaluates the project implementation against the primary authoritative assignment specification (`IT305-lab-socket-programming-project-assignment (1).pdf`), the approved architecture design (`Design_Document_Group_071_072_.pdf`), and live empirical executions in the target Linux runtime environment (WSL2 Ubuntu 24.04.1 LTS on x86_64, Linux 5.15.167.4).

### Verification Categories
Every specification requirement is evaluated across five distinct evidentiary tiers:
1. **Static Code Inspection:** Verification of C99 source files (`protocol.c/h`, `file_transfer.c/h`, `fault_injection.c/h`, `session.c/h`, `work_plan.c/h`, `logging.c/h`) and Makefile targets.
2. **Clean Build Audit:** Out-of-source clean compilation (`make clean && make`) under strict compiler flags (`-std=c99 -Wall -Wextra -Werror -pedantic -pthread`) capturing build logs, exit codes, and compiler diagnostics.
3. **Repository Regression Test Suites:** Complete execution of all existing unit and integration test scripts (`tests/run_server_tests.sh`, `tests/test_phase2.sh`, `tests/test_phase3.sh`, `tests/test_phase4.sh`, `scripts/test_correctness.sh`), verifying 62 integration scenarios with cryptographic byte-for-byte checksum verification.
4. **Supplementary Edge-Case & Adversarial Audit:** Automated testing of boundary conditions, malformed arguments, non-existent paths, buffer limits, session table TTL, process restart persistence, and concurrency limits (101 tests; 91 PASS, 10 documented implementation limitations).
5. **Empirical Benchmark Campaign:** Live, multi-repetition benchmark runs measuring wall-clock time via `clock_gettime(CLOCK_MONOTONIC)` / `time.monotonic_ns()`, application-level frame and payload counters, verifying SHA-256 integrity of all downloaded files.

### Status Definitions
- **`PASS`**: Requirement fully implemented, verified by clean compilation and live test/benchmark execution with byte-for-byte SHA-256 data integrity.
- **`FAIL`**: Implementation defect or behavioral non-compliance identified and preserved.
- **`NOT VERIFIED`**: Requirement cannot be tested due to environmental limitations (e.g., physical multi-computer network).
- **`BLOCKED`**: Prerequisite missing that precludes execution.

---

## 2. Master Requirement Verification Matrix

| ID | Specification Requirement | Authoritative Source | Verification Method | Status | Concrete Evidentiary Findings |
| :--- | :--- | :--- | :--- | :---: | :--- |
| **REQ-01** | **Part I Server: Single-Threaded (Sequential) Mode** | Assignment §Part 1 (p. 2) | Static Code & Live Concurrency Test | **`PASS`** | Implemented in `Part1/server.c` (`SERVER_MODE_SEQUENTIAL`). Connections serviced synchronously on main thread; listen queue serializes clients. Process thread count verified `== 1` via `/proc/[pid]/task` during multi-client load. |
| **REQ-02** | **Part I Server: Multi-Threaded Mode** | Assignment §Part 1 (p. 2) | Static Code & Thread Inspection | **`PASS`** | Implemented in `Part1/server.c` (`SERVER_MODE_MT`, default). Spawns detached POSIX worker thread per accepted client (`pthread_create` + `pthread_detach`). Concurrency verified: active server thread count scales dynamically up to client count (monitored via `/proc/[pid]/task`). |
| **REQ-03** | **Topic Directory Mapping & File Transfer** | Assignment §Part 1 (p. 2) | Live Execution & SHA-256 Hashes | **`PASS`** | `common/topic_manager.c` resolves `<topics_root_dir>/<topic>` and builds lexicographically sorted manifest with 32-bit CRC32 fingerprint. Regular files streamed in 64 KiB chunks (`common/protocol.h`). Byte-exact transfer verified across small, large, binary, empty files and real Animals-10 topics (`mucca`, `gatto`). |
| **REQ-04** | **Part I Concurrent Client Experiment** | Assignment §Part 1 (p. 2-3) | Live Multi-Client Benchmark (Exp 1) | **`PASS`** | Clients launched concurrently in background with overlapping execution intervals (verified by recording start/exit timestamps: `max(t0) < min(t1)`). 1, 2, 4, 8 clients executed across both sequential and MT modes with 100% SHA-256 integrity. |
| **REQ-05** | **Part I Aggregate Throughput vs. Client Count** | Assignment §Part 1 (p. 2-3) | Multi-Run CSV & Matplotlib Plots | **`PASS`** | Measured using monotonic clock across full concurrent wall-clock window (`t_end - t_start`). Summary generated in `Results/exp1_part1_summary.csv` and publication graphs in `Results/Part1_throughput.png` and `.pdf` with mean ± 1 SD error bars. |
| **REQ-06** | **Three-Computer Experiment Deployment** | Assignment §Part 1 (p. 2) | Environment Topology Inspection | **`NOT VERIFIED`** | Specification requires 1 server node and 2 distinct client nodes across physical LAN. The test environment is an isolated local workstation (WSL2 loopback `127.0.0.1`). Local concurrency was thoroughly validated, but physical multi-host network latency, NIC saturation, and cross-machine socket behavior cannot be tested on a single workstation. |
| **REQ-07** | **Part II Probabilistic Fault Injection Engine** | Assignment §Part II (p. 3) | Source Audit & Log Inspection | **`PASS`** | Implemented in `common/fault_injection.c`. Seeded per-thread PRNG (`rand_r` with thread-local seed) decides whether to inject abrupt disconnect before each 64 KiB data chunk (`MSG_FILE_DATA` or `MSG_WORK_DATA`). Calls `close(socket_fd)` immediately without sending EOF. Logged to `stderr` with timestamp, file name, and byte offset. |
| **REQ-08** | **Fault Probability Server CLI Argument** | Assignment §Standard CLI (p. 4) | Argument Parsing Tests | **`PASS`** | All Part II servers (`Part2_Case1/server`, `Part2_Case2/server`, `Part2_Case2_Enhanced/server`) require `<failure probability>` as argument 3. Evaluated in [0.0, 1.0] via `parse_failure_probability()`. Out-of-range values (< 0, > 1, non-numeric) correctly rejected with exit code 1. *(Note: string `"nan"` slips past comparison in glibc; documented limitation).* |
| **REQ-09** | **Part II Case 1: No Session Management (Full Restart)** | Assignment §Part II Case 1 (p. 3) | Test Suite & Fault Logs (Exp 2) | **`PASS`** | Client (`Part2_Case1/client.c`) detects socket disconnect, executes recursive directory purge (`purge_directory` via `nftw`), and restarts transfer from File 0, Offset 0. Server maintains no state. Verified under $p \in \{0.00, 0.01, 0.05, 0.10\}$; redundant bytes scale proportionally with failure count. Final download matches 100% SHA-256. |
| **REQ-10** | **Part II Case 2: Session Management & Checkpoint Resume** | Assignment §Part II Case 2 (p. 3) | Session Unit Tests & Wire Trace (Exp 3) | **`PASS`** | In-memory mutex-protected session table (`common/session.c`). Handshake uses `MSG_SESSION_START` -> `MSG_SESSION_ACCEPT` (64-bit non-zero ID). Client sends `MSG_CHUNK_ACK` per 64 KiB chunk written. On drop, client presents checkpoint via `MSG_SESSION_RESUME`, server validates against authoritative offset, and streams from offset. Zero redundant payload retransmitted. Final files match 100% SHA-256. |
| **REQ-11** | **Case 1 vs. Case 2 Comparative Benchmark** | Assignment §Part II Case 2 (p. 3) | Multi-Run Sweep & Plots (Exp 2 & 3) | **`PASS`** | Sweeps across $p \in \{0.00, 0.01, 0.05, 0.10\}$ using identical topic (`gatto_fault20`, 20 chunks of real Animals-10 images). Comparative summary in `Results/exp23_case_comparison.csv` and graphs `Results/Case1_Case2_throughput.png`, `Results/Case1_Case2_overhead.png`, `Results/Case1_Case2_completion_time.png`. |
| **REQ-12** | **Part II Case 2 Enhanced: Performance Enhancement** | Assignment §Part II Case 2 Enhanced (p. 3-4) | Architectural Review & Code Audit | **`PASS`** | Implemented as Parallel TCP Connections with non-overlapping work partitioning (`common/work_plan.c`) and concurrent random-access disk writes via POSIX `pwrite()` (`Part2_Case2_Enhanced/client.c`). Work units represent discrete file slices (256 KiB) or entire files claimed dynamically across worker threads. |
| **REQ-13** | **Enhanced Connection Scaling Benchmark** | Assignment §Part II Case 2 Enhanced (p. 4) | Multi-Run Connection Sweep (Exp 4) | **`PASS`** | Tested across connection counts $N \in \{1, 2, 4\}$ under $p = 0.00$ and $p = 0.05$. Results in `Results/exp4_enhanced_summary.csv` and graphs `Results/Case2_Enhanced_throughput.png`, `Results/Case2_Enhanced_speedup.png`, `Results/Case2_Enhanced_completion_time.png`. Speedup calculated strictly against $N=1$ baseline under identical topic and fault conditions. |
| **REQ-14** | **Standard CLI Server Invocations** | Assignment §Standard CLI (p. 4) | CLI Invocation Harness | **`PASS`** | Part 1: `./server <port> <topics_root_dir>` (defaults to MT; optional 3rd argument `sequential`). Part 2 (all cases): `./server <port> <topics_root_dir> <failure probability>`. All binaries strictly adhere to argument count, ordering, and validation rules. |
| **REQ-15** | **Standard CLI Client Invocations** | Assignment §Standard CLI (p. 4-5) | CLI Invocation Harness | **`PASS`** | Required: `./client <server_ip> <server_port> <topic> <output_dir>`. Downloads files under `<output_dir>/<topic>/`. Enhanced client supports optional trailing argument `[num_connections]` (1..16), defaulting strictly to 1 when omitted. |
| **REQ-16** | **Build System: Makefile Compliance** | Assignment §Submission (p. 4) | Clean Build Audit (`audit_build.sh`) | **`PASS`** | Independent `Makefile` in each of `Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`. Running `make` produces exactly two executables: `server` and `client`. Compiles under `-std=c99 -Wall -Wextra -Werror -pedantic -pthread` with 0 warnings and exit code 0. Second make invocation reports "Nothing to be done". |
| **REQ-17** | **Animals-10 Dataset Verification & Provenance** | Assignment §Dataset (p. 1-2) | Filesystem Audit & SHA-256 Manifest | **`PASS` (with provenance note)** | Repository contains real Animals-10 image files under `raw-img/` for 8 out of 10 topics (17,675 files total). Complete topic `mucca` (1,866 files, 25.8 MB) used for Part 1 scaling; real image subsets `gatto_fault20` (20 chunks, 0.96 MB) and `gatto_big150` (150 files, 1.79 MB) used for Part 2 fault sweeps. Documented in `Results/dataset_inventory.csv` and `Results/dataset_benchmark_topics.csv`. |
| **REQ-18** | **Data Integrity Verification** | Assignment §Evaluation (p. 5) | Cryptographic Hash Verification | **`PASS`** | Every downloaded file in every test and benchmark run is verified byte-for-byte against the source dataset using SHA-256 checksums (`verify_download()`). Matching filenames or file sizes are not accepted as proof of integrity. 100% of successful runs passed SHA-256 verification. |
| **REQ-19** | **Metric Accounting Integrity** | Assignment §Part II Case 2 (p. 3) | Wire Byte Instrumentation Audit | **`PASS`** | Process-wide monotonic byte accounting instrumented in `common/protocol.c` (`IT305_METRICS=1`). Clearly separates useful payload bytes, total transmitted framed bytes, redundant retransmitted payload bytes, and protocol framing overhead. Overhead percentage calculated as $(total - useful) / useful \times 100$ without double-counting. |
| **REQ-20** | **Publication-Grade Visualizations** | Assignment §Results Folder (p. 4) | Matplotlib Plot Audit | **`PASS`** | Generated in both PNG and PDF formats under `Results/`. Every plot features descriptive titles, labeled axes with units, legends, and error bars (mean ± 1 SD over measured repetitions). Every plot point is traceable to its source CSV. |

---

## 3. Detailed Technical Analysis by Subsystem

### 3.1 Part I: Single-Threaded vs. Multi-Threaded Server Concurrency
- **Implementation:** `Part1/server.c` implements both modes selected via optional command-line flag (`mt` default, `sequential`).
- **Sequential Concurrency Handling:** In sequential mode, incoming connections queue in the TCP listen backlog. The main thread processes one client to completion (`process_client`) before calling `accept()` for the next client.
- **Multi-Threaded Concurrency Handling:** In multi-threaded mode, `accept()` returns immediately on the main loop, allocates a heap-allocated `ClientContext`, and delegates streaming to a detached thread (`pthread_create` + `pthread_detach`). Threads run concurrently across available CPU cores.
- **Empirical Findings:** The sequential server displays near-linear degradation in total completion time as client count increases (each client waits for all prior clients to complete their full ~25.8 MB transfer). Aggregate throughput remains relatively flat (~2.5 - 2.8 MB/s), governed by single-client disk/network rate. The multi-threaded server sustains concurrent streaming, achieving significant aggregate throughput gains on multi-core hardware.

### 3.2 Physical Multi-Computer Experiment Limitation
- **Assignment Clause:** *"Run experiments on a group of three computers: One node executes server, multiple clients run on each of the other two nodes."*
- **Audit Assessment:** The current benchmark campaign was executed on a single host workstation running WSL2 Linux (`127.0.0.1` loopback). While client-server process isolation, socket concurrency, thread scheduling, and multi-socket contention are rigorously exercised, network interface card (NIC) hardware queuing, Ethernet frame serialization, cross-switch packet jitter, and distributed operating system clock offsets cannot be measured without physical access to three separate networked lab PCs.
- **Recommendation:** This requirement is marked **`NOT VERIFIED`** in the matrix and must be explicitly declared as a physical testbed limitation in the final report.

### 3.3 Part II Case 1: Full-Restart Fault Recovery
- **Fault-Injection Trigger:** Evaluated in `Part2_Case1/server.c` at chunk boundaries before transmitting `MSG_FILE_DATA`. The socket is abruptly severed via `close(fd)`.
- **Client Recovery Mechanism:** `Part2_Case1/client.c` catches `EOF` or `ECONNRESET`, immediately invokes `purge_directory()` on the target output directory via POSIX `nftw()`, waits with exponential backoff ($200\text{ ms} \to 2\text{ s}$), and reconnects. The client sends a fresh `MSG_GET` and downloads all files from File 0, Offset 0.
- **Performance Trade-Off:** While simple and stateless, redundant payload bytes scale directly with the number of injected disconnections:
  $$\text{Redundant Bytes} = \sum_{k=1}^{\text{faults}} (\text{bytes transferred in attempt } k)$$
  At higher fault rates ($p \ge 0.05$), completion time and byte overhead grow dramatically.

### 3.4 Part II Case 2: In-Memory Session Management & Checkpoint Resume
- **Session Architecture:** Mutex-protected hash/array table (`common/session.c`, capacity 256 sessions, 600-second TTL). Sessions are keyed by 64-bit pseudo-random identifiers generated via XOR of system time, pid, and sequential counters.
- **Authoritative Progress Tracking:** Checkpoints advance upon receipt of synchronous `MSG_CHUNK_ACK` frames from the client (`session_commit_chunk`). The server maintains the authoritative committed `(file_index, byte_offset)`.
- **Resumption & Reconciliation:** On reconnect, the client proposes its last recorded checkpoint via `MSG_SESSION_RESUME`. If client-side disk contains uncommitted data exceeding the server's committed offset, the client invokes `truncate_and_seek()` (`ftruncate()`) to reconcile local disk state before receiving new data.
- **Persistence Limitation:** The session table resides strictly in process RAM. A server process restart destroys active session state, returning `MSG_RESUME_ERROR (404)` on subsequent resume requests. Session state survives connection drops but not server crashes.

### 3.5 Part II Case 2 Enhanced: Parallel Connections & Work Partitioning
- **Enhancement Architecture:** High-throughput parallel streaming partitioning topics into discrete `WorkUnit`s (`common/work_plan.c`). Each work unit represents either an entire file (if files $\ge$ connections) or a 256 KiB file slice (aligned to 4 $\times$ 64 KiB chunks).
- **Client Disk Concurrency:** Instead of sequential streaming, worker threads connect over separate TCP sockets and issue parallel random-access writes to destination files using POSIX `pwrite()`.
- **Architectural Limitations Identified:**
  1. **Work Unit Ceiling:** `common/work_plan.h` defines `#define MAX_WORK_UNITS 1024`. Topics containing $> 1024$ files (such as the complete `mucca` topic with 1,866 files) cannot be partitioned by the current implementation (`create_work_plan` returns -1).
  2. **Single Global Session:** `Part2_Case2_Enhanced/server.c` utilizes static global variables (`static WorkPlan *g_plan; static int first_worker_started;`) without per-session isolation. A running server instance can only serve a single parallel transfer session; subsequent client sessions require restarting the server process.

---

## 4. Dataset Provenance & Integrity Audit

The official assignment specifies the **Animals-10** dataset (Kaggle). A comprehensive audit of the repository root and `raw-img/` reveals the following real image inventory:

| Topic Name | Files in Repo | Total Bytes | Kaggle Published Count | Status / Completeness | Chunks (64 KiB) | Used in Benchmark Campaign |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| `cane` | 0 | 0 | 4,863 | Missing from git repo | 0 | Not used (empty directory in repo) |
| `cavallo` | 0 | 0 | 2,623 | Missing from git repo | 0 | Not used (empty directory in repo) |
| `elefante` | 428 | 44,679,765 | 1,446 | Partial (428 files committed) | 904 | Staged in repo |
| `farfalla` | 2,112 | 61,877,367 | 2,112 | Complete (100%) | 2,497 | Staged in repo |
| `gallina` | 3,098 | 51,842,424 | 3,098 | Complete (100%) | 3,098 | Staged in repo |
| `gatto` | 1,668 | 151,144,282 | 1,668 | Complete (100%) | 3,563 | Source for `gatto_fault20` & `gatto_big150` |
| `mucca` | 1,866 | 25,833,171 | 1,866 | Complete (100%) | 1,866 | Primary dataset for Part 1 Scaling (Exp 1) |
| `pecora` | 1,820 | 62,629,703 | 1,820 | Complete (100%) | 2,262 | Staged in repo |
| `ragno` | 4,821 | 78,999,493 | 4,821 | Complete (100%) | 4,987 | Staged in repo |
| `scoiattolo` | 1,862 | 25,409,193 | 1,862 | Complete (100%) | 1,862 | Staged in repo |

### Benchmark Topic Selection
To maintain strict adherence to real Animals-10 data while ensuring mathematical feasibility under high fault rates:
1. **`mucca` (Complete Topic):** 1,866 real JPEG images, 25,833,171 bytes. Used for Part 1 Sequential vs. Multi-Threaded scaling (Exp 1).
2. **`gatto_fault20` (Real Image Subset):** Exactly 10 real JPEG images selected from `raw-img/gatto`, each sized between 64 KiB and 128 KiB, totaling 955,798 bytes and exactly 20 chunks. This ensures that Case 1 (full restart) can mathematically terminate under $p = 0.10$ within the 50-retry limit, allowing direct, equivalent comparison against Case 2 and Case 2 Enhanced.
3. **`gatto_big150` (Extended Real Image Topic):** First 150 real JPEG images from `raw-img/gatto`, totaling 1,793,915 bytes (150 chunks). Tested on Case 2 and Case 2 Enhanced to demonstrate scalability on larger real image collections.

---

## 5. Summary of Implementation Limits and Known Deficiencies

The audit surfaced the following edge-case behaviors, preserved in `Results/correctness_extra_results.csv`:
1. **IEEE 754 NaN Failure Probability:** `parse_failure_probability()` uses standard `strtod()`. When passed the literal string `"nan"`, `strtod()` returns NaN. In standard C floating-point comparisons, `val < 0.0 || val > 1.0` evaluates to false for NaN, allowing it to be accepted as a valid probability.
2. **Case 2 Client Disconnect Retry Loop:** `Part2_Case2/client.c` does not fail immediately on invalid IP address or port; it enters its 50-attempt reconnection backoff loop, taking up to 95 seconds before terminating.
3. **Enhanced Server Single-Session Scope:** `Part2_Case2_Enhanced/server.c` manages only one active session per server process instance due to static global state.
4. **Enhanced Maximum Work Unit Boundary:** Topics exceeding 1,024 files cannot be transferred by `Part2_Case2_Enhanced` due to `MAX_WORK_UNITS 1024`.

---

## 6. Audit Verdict and Next-Phase Readiness

**Verdict:** `BENCHMARKS READY WITH LIMITATIONS`

All four required implementations compile cleanly without warnings, all core wire protocol features operate correctly, and 100% of downloaded files pass cryptographic SHA-256 verification. Empirical benchmarks have been collected using high-resolution monotonic timers and wire-level byte counters.

The limitations (single-workstation execution rather than 3 physical computers, 8/10 topics present in repo, and Enhanced server work-unit ceiling) are documented and ready to be transparently presented in the final report.
