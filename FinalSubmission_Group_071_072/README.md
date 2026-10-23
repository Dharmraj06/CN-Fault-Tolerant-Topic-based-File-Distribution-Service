# Fault-Tolerant Topic-Based File Distribution Service

**Course:** IT305 Computer Networks Laboratory (Autumn 2026)  
**Institution:** Dhirubhai Ambani Institute of Information and Communication Technology (DA-IICT)  
**Merged Groups:** Group 071 & Group 072  

## 1. Project Purpose

This project implements a high-performance, fault-tolerant topic-based file distribution service in C using raw POSIX TCP sockets (`sys/socket.h`, `pthread.h`). The system provides reliable directory-level distribution of multi-file topics without external application frameworks.

The implementation evaluates three key architectural dimensions:
1. **Concurrency (Part I):** Single-threaded sequential vs. multi-threaded per-client worker pools.
2. **Fault Tolerance (Part II):** Unmanaged full transfer restart (Case 1) vs. stateful checkpoint-based session resumption (Case 2) under probabilistic network disconnections.
3. **Parallel Transfer (Case 2 Enhanced):** Multi-socket parallel TCP connection work-unit partitioning with concurrent random-access disk reassembly using `pwrite()`.

---

## 2. Team Members & Contributions

| Member Name | Roll Number | Merged Group | Primary Project Contributions |
| :--- | :---: | :---: | :--- |
| **Jay Vaghela** | `202401237` | 071 | Part 1 sequential & multi-threaded server architecture; socket lifecycle; thread dispatching; baseline concurrency benchmarks. |
| **Dharmraj Vaghela** | `202401236` | 071 | Part 2 Case 2 checkpoint-resume protocol; authoritative session state table; thread-safe mutex locking; wire telemetry instrumentation. |
| **Vivek Dodiya** | `202401253` | 071 | Part 2 Case 1 full-restart recovery mechanism; destination directory purging; file truncation logic; fault injection test verification. |
| **Atharv Trivedi** | `202401230` | 072 | Part 2 Case 2 Enhanced parallel connection engine; dynamic work unit planning; chunk slicing; multi-connection reassembly using `pwrite()`. |
| **Shlok Topaliya** | `202401229` | 072 | Automated regression test suites (62 core tests); supplementary edge-case test harnesses (101 scenarios); SHA-256 integrity auditing pipelines. |
| **Yug Malaviya** | `202401256` | 072 | Experimental campaign orchestration (176 verified runs); statistical metric computation (throughput, overhead, speedup, efficiency); publication graph generation. |

---

## 3. Directory Structure

```text
FinalSubmission_Group_071_072/
├── FinalReport_Group_071_072.pdf   # 21-page comprehensive final academic report
├── README.md                       # This self-contained submission documentation
├── Part1/                          # Part 1: Baseline Sequential and Multi-Threaded Server & Client
│   ├── Makefile                    # Strict C99 build script produces 'server' and 'client'
│   ├── server.c                    # Part 1 server implementation
│   └── client.c                    # Part 1 client implementation
├── Part2_Case1/                    # Part 2 Case 1: Full Transfer Restart on Failure
│   ├── Makefile
│   ├── server.c                    # Multi-threaded server with probabilistic fault injection
│   └── client.c                    # Client with directory purging and complete restart
├── Part2_Case2/                    # Part 2 Case 2: Stateful Checkpoint-Based Resumption
│   ├── Makefile
│   ├── server.c                    # Server with in-memory session table and chunk ACK commits
│   └── client.c                    # Client with session resume handshake and offset seek
├── Part2_Case2_Enhanced/           # Part 2 Case 2 Enhanced: Parallel Multi-Socket TCP Distribution
│   ├── Makefile
│   ├── enhanced_protocol.h         # Work plan and parallel connection frame definitions
│   ├── server.c                    # Enhanced multi-connection work unit dispatch server
│   └── client.c                    # Enhanced parallel client with multi-thread pwrite() reassembly
├── common/                         # Shared modular C99 libraries
│   ├── protocol.h / protocol.c     # 8-byte binary framing, magic synchronization, send/recv exact
│   ├── topic_manager.h / .c        # Directory scanning, topic cataloging, manifest hashing
│   ├── fault_injection.h / .c      # Bernoulli disruption engine and float parser
│   ├── session.h / .c              # Authoritative session state table and mutex locks
│   ├── work_plan.h / .c            # Work unit partitioning and chunk range slicing
│   └── logging.h / .c              # Thread-safe console and telemetry logging
└── Results/                        # Experimental artifacts and verified benchmark data
    ├── Part1_throughput.png / .pdf             # Part 1 concurrency scaling graph
    ├── Case1_Case2_throughput.png / .pdf       # Case 1 vs. Case 2 throughput graph
    ├── Case1_Case2_completion_time.png / .pdf  # Case 1 vs. Case 2 completion time graph
    ├── Case1_Case2_redundant_bytes.png / .pdf  # Case 1 vs. Case 2 redundant retransmission graph
    ├── Case1_Case2_overhead.png / .pdf         # Case 1 vs. Case 2 wire overhead graph
    ├── Case2_Enhanced_speedup.png / .pdf       # Enhanced parallel speedup graph
    ├── Case2_Enhanced_throughput.png / .pdf    # Enhanced throughput scaling graph
    ├── Case2_Enhanced_completion_time.png/.pdf # Enhanced completion time graph
    ├── benchmark_run_manifest.csv              # Master manifest of 176 unique benchmark runs
    ├── exp1_part1_raw.csv / summary.csv        # Part 1 raw (32 runs) and summary tables
    ├── exp2_case1_raw.csv / summary.csv        # Case 1 raw (24 runs) and summary tables
    ├── exp3_case2_raw.csv / summary.csv        # Case 2 raw (36 runs) and summary tables
    ├── exp23_case_comparison.csv               # Unified Case 1 vs. Case 2 comparison table
    ├── exp4_enhanced_raw.csv / summary.csv     # Enhanced raw (84 runs) and summary tables
    ├── correctness_verification.log            # 62/62 core regression test execution log
    ├── correctness_extra.log / results.csv     # 101/101 supplementary edge-case audit log
    ├── dataset_inventory.csv                   # Audit of 17,675 authentic Animals-10 images
    ├── final_audit_corrections.md              # Reconciled audit report and defect resolutions
    ├── final_requirement_status.md             # Formal assignment requirement traceability
    └── final_validation_summary.txt            # Final system validation summary
```

---

## 4. Prerequisites & Build Instructions

### Prerequisites
- **Compiler:** GCC with C99 support (`gcc >= 9.0`).
- **Operating System:** Linux (native or WSL2 / Ubuntu 20.04+).
- **Libraries:** POSIX standard C runtime with `pthreads` (`-pthread`) and `libm` (`-lm`).

### Building All Four Variants
Each directory contains a standalone `Makefile` compiling under strict C99 flags:
```bash
gcc -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I../common
```

To build all variants from the submission root:
```bash
# Build Part 1
make -C Part1

# Build Part 2 Case 1
make -C Part2_Case1

# Build Part 2 Case 2
make -C Part2_Case2

# Build Part 2 Case 2 Enhanced
make -C Part2_Case2_Enhanced
```

In every directory, compilation produces exactly two executables: `server` and `client` with **zero warnings and exit code 0**.

To clean binaries and object files:
```bash
make -C Part1 clean
make -C Part2_Case1 clean
make -C Part2_Case2 clean
make -C Part2_Case2_Enhanced clean
```

---

## 5. Execution Syntax (Auto-Grader Compliant)

All executables adhere strictly to the standard command-line interface mandated by the project specification.

### Part 1: Baseline Service
```bash
# Start Server (defaults to Multi-Threaded; specify 'seq' for sequential mode)
./Part1/server <port> <topics_root_dir> [mt|seq]

# Run Client (fetches topic and creates <output_dir>/<topic>/)
./Part1/client <server_ip> <server_port> <topic> <output_dir>
```

### Part 2 Case 1: Full Transfer Restart
```bash
# Start Server with fault injection probability p in [0.0, 1.0]
./Part2_Case1/server <port> <topics_root_dir> <failure_probability>

# Run Client
./Part2_Case1/client <server_ip> <server_port> <topic> <output_dir>
```

### Part 2 Case 2: Stateful Checkpoint Resume
```bash
# Start Server with fault injection probability p in [0.0, 1.0]
./Part2_Case2/server <port> <topics_root_dir> <failure_probability>

# Run Client
./Part2_Case2/client <server_ip> <server_port> <topic> <output_dir>
```

### Part 2 Case 2 Enhanced: Parallel Multi-Socket TCP
```bash
# Start Enhanced Server with fault injection probability p in [0.0, 1.0]
./Part2_Case2_Enhanced/server <port> <topics_root_dir> <failure_probability>

# Run Enhanced Client (defaults to 1 connection; optionally pass -n <num_connections>)
./Part2_Case2_Enhanced/client <server_ip> <server_port> <topic> <output_dir> [-n <conns>]
```

---

## 6. Expected Dataset Directory Layout

The server expects `<topics_root_dir>` to contain one subdirectory per topic. The subdirectory name serves as the topic key:
```text
<topics_root_dir>/
├── mucca/              # Topic key 'mucca'
│   ├── e001.jpg
│   └── ... (files)
├── gatto/              # Topic key 'gatto'
│   └── ... (files)
└── elefante/           # Topic key 'elefante'
    └── ... (files)
```

Upon successful transfer, the client populates `<output_dir>/<topic>/` with all files belonging to that topic:
```text
<output_dir>/
└── <topic>/
    ├── e001.jpg
    └── ... (identical files matching server SHA-256 hashes)
```

---

## 7. Fault Injection Mechanism

The fault injection engine (`common/fault_injection.c`) intercepts every 64 KiB `MSG_FILE_DATA` chunk dispatched by the server:
1. For each chunk, the server draws a random variable $U \sim \text{Uniform}(0, 1)$.
2. If $U < p$ (where $p$ is the command-line failure probability), the server logs the injection event and immediately invokes `close(client_socket)`.
3. This abrupt socket closure simulates an immediate TCP RST/FIN or link drop mid-transfer.
4. **Case 1 Client:** Purges the local destination folder and requests the entire topic from file 0, offset 0.
5. **Case 2 Client:** Reconnects, submits its last acknowledged chunk checkpoint (`MSG_SESSION_RESUME`), and resumes transfer from the exact byte offset without duplicate payload transmission.

---

## 8. Where to Find Results and the Final Report

- **Final Report PDF:** `FinalReport_Group_071_072.pdf` at the root of the submission archive. Contains full architecture diagrams, implementation analysis, test results, and 21 pages of academic evaluation.
- **Summary CSVs:** `Results/exp1_part1_summary.csv`, `Results/exp23_case_comparison.csv`, `Results/exp4_enhanced_summary.csv`.
- **Raw CSV Data:** `Results/exp1_part1_raw.csv`, `Results/exp2_case1_raw.csv`, `Results/exp3_case2_raw.csv`, `Results/exp4_enhanced_raw.csv`.
- **Master Manifest:** `Results/benchmark_run_manifest.csv` (176 unique verified runs).
- **Performance Plots:** `Results/*.png` and `Results/*.pdf`.

---

## 9. Transparent Limitations Disclosure

In accordance with rigorous academic integrity standards:
1. **Three-Computer Physical LAN Deployment:** All benchmarks and regression tests were conducted in a single-host WSL2 environment (Ubuntu 24.04 on Linux 5.15) over the local loopback interface (`127.0.0.1`, MTU 65,536). Physical deployment across three independent hardware machines connected via LAN remains **`NOT VERIFIED`**.
2. **Animals-10 Dataset Coverage:** 8 authentic classes are present with 17,675 authentic JPEG images (257.6 MB). Classes `cane` and `cavallo` exist as empty directories and were never committed to the repository Git history. No synthetic images were fabricated.
3. **Enhanced Work Unit Bound:** Case 2 Enhanced enforces `MAX_WORK_UNITS = 1024` in `common/work_plan.h`. Topics containing more than 1,024 files/ranges are safely rejected with an error exit code.
