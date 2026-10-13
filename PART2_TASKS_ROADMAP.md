# IT305 Computer Networks Lab: Part 2 Execution Roadmap & Task Allocation
**Project:** Fault-Tolerant Topic-Based File Distribution Service  
**Academic Group:** Merged Group 071 + Group 072 (DA-IICT)  
**Execution Window:** 13 October 2026 – 15 October 2026  
**Final Working Deadline:** 15 October 2026 (23:59 IST)

---

## 1. Project Overview & Current Status

### 1.1 Current Status (Repository & Workspace Inspection)
- **Repository Inspection:** Clean slate on `main` branch with base configuration. Part 1 has been completed by the team and its foundational modules (`protocol`, `topic_manager`, `file_transfer`, `logging`, sequential/multithreaded server and client) are ready for direct reuse.
- **Part 2 Implementation Needed:**
  - **Part2_Case1:** Multithreaded server with probabilistic fault injection (`fault_injection.*`); client detects abrupt connection drops, purges incomplete files, and restarts topic transfer from file 0, offset 0.
  - **Part2_Case2:** Thread-safe session management (`session.*`) with 64-bit session ID, per-chunk acknowledgments (`CHUNK_ACK`), server-authoritative checkpoint commit, and client resume (`SESSION_RESUME` / `RESUME_ACCEPT`) from validated byte offsets.
  - **Part2_Case2_Enhanced:** Work-unit partitioning (`work_plan.*`) across parallel TCP connections, concurrent non-overlapping file writes using `pwrite()`, independent connection recovery, retaining default single-connection CLI invocation.
  - **Evaluation & Packaging:** Automated correctness verification, benchmark execution across fault rates (`0%`, `1%`, `5%`, `10%`), CSV metric generation, graph plotting in `Results/`, and submission report compilation (`FinalReport_Group_071_072.pdf`).

### 1.2 Strict Non-Overlapping Ownership Matrix
To guarantee zero merge conflicts and clear technical accountability, each member has exclusive ownership over dedicated files and modules:

| Member | Student ID | Exclusive Subsystem Ownership | Exclusive File Ownership |
|---|---|---|---|
| **Jay Vaghela** | 202401237 | Protocol Wire Framing & Topic Manifest Engine | `common/protocol.h`, `common/protocol.c`<br>`common/topic_manager.h`, `common/topic_manager.c`<br>`Part1/server.c`, `Part1/Makefile` |
| **Atharv Trivedi** | 202401230 | Fault Injection Subsystem & Part 2 Case 1 End-to-End | `common/fault_injection.h`, `common/fault_injection.c`<br>`Part2_Case1/server.c`, `Part2_Case1/client.c`<br>`Part2_Case1/Makefile` |
| **Dharmraj Vaghela** | 202401236 | Thread-Safe Session Engine & Part 2 Case 2 Server | `common/session.h`, `common/session.c`<br>`Part2_Case2/server.c`, `Part2_Case2/Makefile` |
| **Vivek Dodiya** | 202401253 | Bounded File I/O Engine & Part 2 Case 2 Client | `common/file_transfer.h`, `common/file_transfer.c`<br>`Part1/client.c`, `Part2_Case2/client.c` |
| **Shlok Topaliya** | 202401229 | Parallel TCP Coordinator & Part 2 Case 2 Enhanced | `common/work_plan.h`, `common/work_plan.c`<br>`Part2_Case2_Enhanced/server.c`<br>`Part2_Case2_Enhanced/client.c`<br>`Part2_Case2_Enhanced/Makefile` |
| **Yug Malaviya** | 202401256 | Logging, Test Harness, Benchmarking & Submission Package | `common/logging.h`, `common/logging.c`<br>`scripts/test_correctness.sh`<br>`scripts/run_benchmarks.py`<br>`scripts/plot_results.py`<br>`scripts/package_submission.sh`<br>`Results/*`, `FinalReport_Group_071_072.pdf` |

---

## 2. Day-Wise Task Assignment

### Day 1: 13 October 2026 — Implementation and Initial Integration

| Member | Exact Tasks | Files/Modules | Expected Deliverable |
|---|---|---|---|
| **Jay Vaghela** | 1. Import Part 1 `protocol.h`/`.c` and implement Part 2 wire frame definitions (`SESSION_START`, `SESSION_ACCEPT`, `CHUNK_ACK`, `SESSION_RESUME`, `RESUME_ACCEPT`).<br>2. Build `common/topic_manager.h`/`.c` with directory traversal, lexicographical file sorting, and path traversal security guards (`/`, `..`). | `common/protocol.h`<br>`common/protocol.c`<br>`common/topic_manager.h`<br>`common/topic_manager.c`<br>`Part1/server.c` | Frozen wire protocol header and topic manifest generator compiling cleanly. |
| **Atharv Trivedi** | 1. Implement `common/fault_injection.h`/`.c` with command-line probability parsing `[0.0, 1.0]`, per-chunk PRNG evaluation, and abrupt socket close.<br>2. Implement `Part2_Case1/server.c` integrating the fault injection check before each `FILE_DATA` chunk. | `common/fault_injection.h`<br>`common/fault_injection.c`<br>`Part2_Case1/server.c`<br>`Part2_Case1/Makefile` | Working fault-injection helper module and compiling Case 1 server executable. |
| **Dharmraj Vaghela** | 1. Implement thread-safe in-memory session table (`common/session.h`/`.c`) with `Session` struct, 64-bit session ID generation, and POSIX mutex locking.<br>2. Implement `Part2_Case2/server.c` connection bootstrap and `SESSION_START` handler returning `SESSION_ACCEPT`. | `common/session.h`<br>`common/session.c`<br>`Part2_Case2/server.c`<br>`Part2_Case2/Makefile` | Thread-safe session management module; Case 2 server accepting connections and registering active sessions. |
| **Vivek Dodiya** | 1. Import `common/file_transfer.h`/`.c` with chunked streaming loops (`send_file_chunk`, `receive_file_chunk`, `send_all`, `recv_all`).<br>2. Implement `Part2_Case2/client.c` initial connection handshake, storing `session_id` and emitting `CHUNK_ACK` upon successful disk writes. | `common/file_transfer.h`<br>`common/file_transfer.c`<br>`Part1/client.c`<br>`Part2_Case2/client.c` | Verified bounded file streaming module; Case 2 client receiving chunks and transmitting per-chunk ACKs. |
| **Shlok Topaliya** | 1. Implement `common/work_plan.h`/`.c` data structures to split topic files and chunk byte ranges into distinct `work_unit_t` objects.<br>2. Scaffold `Part2_Case2_Enhanced/server.c` and `client.c` directory structure, build flags, and baseline connection listener. | `common/work_plan.h`<br>`common/work_plan.c`<br>`Part2_Case2_Enhanced/Makefile` | Work-plan partitioning interface and compiling build scaffold for the Enhanced case. |
| **Yug Malaviya** | 1. Implement thread-safe `common/logging.h`/`.c` with nanosecond monotonic timers (`CLOCK_MONOTONIC`) and byte counter synchronization.<br>2. Write baseline test suite `scripts/test_correctness.sh` with synthetic test files (including 0-byte and binary files). | `common/logging.h`<br>`common/logging.c`<br>`scripts/test_correctness.sh` | Operational thread-safe logger; automated bash script to verify basic file transfer correctness. |

---

### Day 2: 14 October 2026 — Resume, Enhanced Case, and Testing

| Member | Exact Tasks | Files/Modules | Expected Deliverable |
|---|---|---|---|
| **Jay Vaghela** | 1. Verify Part 1 sequential vs multi-threaded server execution and benchmark baseline aggregate file transfer rate.<br>2. Validate header serialization and deserialization against boundary edge cases (short reads, network endianness conversions). | `Part1/server.c`<br>`Part1/Makefile`<br>`common/protocol.c` | Part 1 baseline verified; robust protocol parser resilient against partial TCP chunks. |
| **Atharv Trivedi** | 1. Implement `Part2_Case1/client.c` detecting socket closure, purging incomplete destination directories, and restarting topic from file 0, offset 0.<br>2. Add bounded retry count and backoff in Case 1 client to prevent infinite loops during high failure rates (`prob >= 0.5`). | `Part2_Case1/client.c`<br>`Part2_Case1/Makefile` | Fully operational Case 1 client with clean directory wipe and reliable restart loop. |
| **Dharmraj Vaghela** | 1. Implement `commit_checkpoint` and `validate_resume` in `common/session.c` under fine-grained mutex locks.<br>2. Implement `SESSION_RESUME` handler in `Part2_Case2/server.c` seeking to authoritative checkpoint and resuming file transfer without retransmission. | `common/session.c`<br>`Part2_Case2/server.c` | Case 2 server capable of validating reconnecting clients and resuming from exact uncommitted byte boundaries. |
| **Vivek Dodiya** | 1. Implement reconnection loop in `Part2_Case2/client.c` sending `SESSION_RESUME` with last acknowledged checkpoint.<br>2. Implement client destination file seek and truncation logic to match server-authoritative offset before resuming receipt. | `Part2_Case2/client.c`<br>`Part2_Case2/Makefile` | Fully operational Case 2 client with verified zero redundant byte reception after network drop. |
| **Shlok Topaliya** | 1. Implement multi-socket worker handling in `Part2_Case2_Enhanced/server.c` serving work units across parallel TCP connections.<br>2. Implement connection pool coordinator in `Part2_Case2_Enhanced/client.c` using `pwrite()` to write non-overlapping ranges concurrently. | `Part2_Case2_Enhanced/server.c`<br>`Part2_Case2_Enhanced/client.c`<br>`Part2_Case2_Enhanced/work_plan.c` | Fully functional Case 2 Enhanced implementation executing parallel file transfers without file corruption. |
| **Yug Malaviya** | 1. Implement automated benchmark harness `scripts/run_benchmarks.py` executing test runs for Case 1 and Case 2 across fault probabilities (`0%`, `1%`, `5%`, `10%`).<br>2. Log raw measurements (completion time, useful bytes, redundant bytes, throughput) to `Results/*.csv`. | `scripts/run_benchmarks.py`<br>`Results/case1_benchmark.csv`<br>`Results/case2_benchmark.csv` | Automated benchmark pipeline and initial raw experimental CSV logs for Case 1 and Case 2. |

---

### Day 3: 15 October 2026 — Final Integration, Benchmarking, and Submission

| Member | Exact Tasks | Files/Modules | Expected Deliverable |
|---|---|---|---|
| **Jay Vaghela** | 1. Perform static analysis across all server code; eliminate all compiler warnings (`gcc -Wall -Wextra -pedantic`).<br>2. Audit server CLI arguments across all four directories to ensure strict compliance: `./server <port> <root_dir> <failure_prob>`. | `Part1/server.c`<br>`Part2_Case1/server.c`<br>`Part2_Case2/server.c`<br>`Part2_Case2_Enhanced/server.c` | Warning-free server compilation across all 4 directories with 100% compliant CLI interface. |
| **Atharv Trivedi** | 1. Perform static analysis across all client code; audit client CLI arguments: `./client <ip> <port> <topic> <output_dir>`.<br>2. Verify strict client output directory layout: confirm received files are placed in `<output_dir>/<topic>/`. | `Part1/client.c`<br>`Part2_Case1/client.c`<br>`Part2_Case2/client.c`<br>`Part2_Case2_Enhanced/client.c` | Warning-free client compilation across all 4 directories with verified destination directory hierarchies. |
| **Dharmraj Vaghela** | 1. Execute edge-case tests: invalid topic, non-existent directories, 0-byte files, and extreme failure probabilities (`0.0` and `1.0`).<br>2. Create automated packaging script `scripts/package_submission.sh` generating `FinalSubmission_Group_071_072.tar.gz`. | `scripts/package_submission.sh`<br>`FinalSubmission_Group_071_072.tar.gz` | Edge-case verification report; automated packaging script producing standard submission archive. |
| **Vivek Dodiya** | 1. Run memory leak and file descriptor leak audits using Valgrind on client and server processes during active recovery.<br>2. Verify SIGPIPE signal suppression (`signal(SIGPIPE, SIG_IGN)`) and clean socket descriptor closure on disconnects. | All C source files<br>`docs/memory_audit_report.txt` | Verified zero-leak memory and socket descriptor audit across all executables. |
| **Shlok Topaliya** | 1. Benchmark Case 2 Enhanced against Case 2 baseline (comparing 1, 2, and 4 parallel TCP connections) and record metrics.<br>2. Implement `scripts/plot_results.py` to generate throughput and overhead comparison plots in `Results/`. | `scripts/plot_results.py`<br>`Results/*_throughput.png`<br>`Results/*_overhead.png` | Complete set of publication-grade PNG/PDF performance plots generated from authentic CSV logs. |
| **Yug Malaviya** | 1. Write consolidated final report `FinalReport_Group_071_072.pdf` (cover page, member contributions, design deviations, results tables & graphs).<br>2. Write root `README.md` with build and execution instructions; conduct final end-to-end dry-run verification. | `FinalReport_Group_071_072.pdf`<br>`README.md` | Final consolidated project report and root README ready for final submission. |

---

## 3. Integration & Peer Verification Plan

### 3.1 Non-Overlapping Subsystem Ownership
- **Part 1 Baseline:** Jay Vaghela (Server) & Vivek Dodiya (Client)
- **Part 2 Case 1:** Atharv Trivedi (Server, Client & Fault Injection)
- **Part 2 Case 2:** Dharmraj Vaghela (Server & Session Engine) & Vivek Dodiya (Client & Resume Engine)
- **Part 2 Case 2 Enhanced:** Shlok Topaliya (Server, Client & Work Planner)
- **Quality Assurance & Submission:** Yug Malaviya (Testing, Benchmarking, Report)

### 3.2 Integration Milestones
- **Milestone 1 (13 Oct, 20:00 IST):** Header Freeze — All common headers (`protocol.h`, `session.h`, `file_transfer.h`, `fault_injection.h`, `logging.h`, `work_plan.h`) finalized. No changes to function prototypes permitted after this point.
- **Milestone 2 (14 Oct, 14:00 IST):** Case 1 & Case 2 Operational — Case 1 restarts reliably; Case 2 resumes from byte offset without transmitting redundant bytes.
- **Milestone 3 (14 Oct, 22:00 IST):** Case 2 Enhanced Operational — Parallel TCP transfer completes without corruption; default 1-connection mode behaves identically to Case 2.
- **Milestone 4 (15 Oct, 18:00 IST):** Experimental Data Frozen — All CSV benchmarks completed on reference dataset (`animals10`); graphs generated.
- **Milestone 5 (15 Oct, 22:00 IST):** Final Submission Freeze — Clean build of all 4 directories, automated checksum validation passed, archive packaged.

---

## 4. Final Submission Checklist (Before 15 October Ends)

- [ ] **Directory Layout:** Required folders exist: `Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`, and `Results/`.
- [ ] **Independent Builds:** Running `make` in each folder produces exactly two executables named `server` and `client`.
- [ ] **Server CLI Compliance:**
  - Part 1: `./server <port> <topics_root_dir>`
  - Part 2 (all cases): `./server <port> <topics_root_dir> <failure probability>`
- [ ] **Client CLI Compliance:**
  - All cases: `./client <server_ip> <server_port> <topic> <output_dir>` (Enhanced permits optional trailing connection count, defaulting to 1).
- [ ] **Destination Layout:** Client writes received files strictly under `<output_dir>/<topic>/`.
- [ ] **No Hard-Coded Values:** Zero hard-coded ports, IP addresses, or dataset paths in C code.
- [ ] **Network Robustness:** `SIGPIPE` ignored (`signal(SIGPIPE, SIG_IGN)`); partial reads and writes wrapped in `send_all()` / `recv_all()`.
- [ ] **Binary Correctness:** Byte-for-byte identity confirmed (`diff -r` / SHA256) between source topic directory and client output directory across all fault probabilities.
- [ ] **Experimental Results:** Raw `.csv` logs and required throughput/overhead graphs (`.png` / `.pdf`) present in `Results/`.
- [ ] **Documentation:** `FinalReport_Group_071_072.pdf` complete with member contribution breakdown; `README.md` includes explicit build/run commands.
- [ ] **Archive Structure:** Final archive `FinalSubmission_Group_071_072.tar.gz` (or `.zip`) contains only required folders and compiles cleanly out of the box.
