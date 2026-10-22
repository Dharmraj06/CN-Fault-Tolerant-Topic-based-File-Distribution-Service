# Fault-Tolerant Topic-Based File Distribution Service

**Course:** IT305 Computer Networks Lab (Autumn 2026)  
**Institution:** Dhirubhai Ambani University (Formerly DA-IICT), Gandhinagar  
**Merged Group:** Group 071 + Group 072  

## Team Members & Phase Ownership

| Phase | Member Name | Roll Number | Core Subsystem & Deliverables Owned |
| :---: | :--- | :---: | :--- |
| **Phase 1** | **Jay Vaghela** | `202401237` | 16-Byte Binary Framing (`common/protocol.*`), Topic Manifest & Path Security (`common/topic_manager.*`), `Part1/server.c` |
| **Phase 2** | **Vivek Dodiya** | `202401253` | Bounded 64 KiB File Streaming Engine (`common/file_transfer.*`), `Part1/client.c`, `Part1/Makefile` |
| **Phase 3** | **Atharv Trivedi** | `202401230` | Thread-Safe Probabilistic Fault Engine (`common/fault_injection.*`), `Part2_Case1/` Server & Full-Restart Client |
| **Phase 4** | **Dharmraj Vaghela** | `202401236` | Mutex-Protected In-Memory Session Table (`common/session.*`), `Part2_Case2/` Checkpoint Server & Resume Client |
| **Phase 5** | **Shlok Topaliya** | `202401229` | Disjoint Work-Unit Partitioning (`common/work_plan.*`), `Part2_Case2_Enhanced/` Parallel TCP Server & `pwrite()` Client |
| **Phase 6** | **Yug Malaviya** | `202401256` | Thread-Safe Monotonic Logger (`common/logging.*`), Regression & Benchmark Suite (`scripts/*`, `Results/*`), `FinalReport_Group_071_072.pdf` |

## Directory Structure

- `Part1/` — Baseline topic file distribution service (supports both Multi-Threaded and Sequential server modes).
- `Part2_Case1/` — Multi-threaded server with probabilistic fault injection and stateless client recovery (directory purge + full restart from File 0, Offset 0).
- `Part2_Case2/` — Multi-threaded server with in-memory session management (`SESSION_START`, `CHUNK_ACK`, `SESSION_RESUME`) and mid-file checkpoint resumption with `ftruncate()` reconciliation.
- `Part2_Case2_Enhanced/` — High-performance multi-socket parallel TCP transfer partitioning topics into non-overlapping `WorkUnit`s written concurrently via POSIX `pwrite()`.
- `common/` — Shared C99 modules (`protocol.*`, `topic_manager.*`, `file_transfer.*`, `fault_injection.*`, `session.*`, `work_plan.*`, `logging.*`).
- `Results/` — Raw CSV benchmark measurements (`exp1..exp4_*.csv`) and publication-grade PNG performance plots.
- `scripts/` — Automated correctness regression suite (`test_correctness.sh`), benchmark runner (`run_benchmarks.py`), plot generator (`plot_results.py`), and clean-sandbox submission packager (`package_submission.sh`).
- `FinalReport_Group_071_072.pdf` — Consolidated final project report containing architecture specifications, design refinements, and experimental performance analysis.

## Build Instructions

Each implementation directory contains an independent `Makefile` compiling under strict C99 flags (`gcc -std=c99 -Wall -Wextra -Werror -pedantic -pthread`) and producing two executables named `server` and `client`:

```bash
make -C Part1
make -C Part2_Case1
make -C Part2_Case2
make -C Part2_Case2_Enhanced
```

To clean intermediate `.o` files and binaries in any directory:

```bash
make -C Part1 clean
make -C Part2_Case1 clean
make -C Part2_Case2 clean
make -C Part2_Case2_Enhanced clean
```

## Standard Command-Line Invocations (Auto-Grader Compliant)

### 1. Part 1 (Baseline Service)
```bash
# Start Part 1 Server (defaults to Multi-Threaded; pass optional 3rd arg 'sequential' for single-threaded mode)
./Part1/server <port> <topics_root_dir> [mt|sequential]

# Run Part 1 Client (downloads all regular files in <topics_root_dir>/<topic> into <output_dir>/<topic>/)
./Part1/client <server_ip> <server_port> <topic> <output_dir>
```

### 2. Part 2 — Case 1 (No Session Management / Full Topic Restart)
```bash
# Start Case 1 Server with chunk failure probability in [0.0, 1.0]
./Part2_Case1/server <port> <topics_root_dir> <failure probability>

# Run Case 1 Client (purges <output_dir>/<topic>/ and restarts from File 0, Offset 0 on disconnect)
./Part2_Case1/client <server_ip> <server_port> <topic> <output_dir>
```

### 3. Part 2 — Case 2 (In-Memory Session Management & Checkpoint Resume)
```bash
# Start Case 2 Server with chunk failure probability in [0.0, 1.0]
./Part2_Case2/server <port> <topics_root_dir> <failure probability>

# Run Case 2 Client (acknowledges chunks via MSG_CHUNK_ACK and resumes from last committed offset)
./Part2_Case2/client <server_ip> <server_port> <topic> <output_dir>
```

### 4. Part 2 — Case 2 Enhanced (Parallel TCP Connections & Concurrent `pwrite()`)
```bash
# Start Case 2 Enhanced Server with chunk failure probability in [0.0, 1.0]
./Part2_Case2_Enhanced/server <port> <topics_root_dir> <failure probability>

# Run Case 2 Enhanced Client
# 1) Default 1-connection transfer (auto-grader compliant, 4 args):
./Part2_Case2_Enhanced/client <server_ip> <server_port> <topic> <output_dir>

# 2) Explicit parallel connection count (1..16 connections):
./Part2_Case2_Enhanced/client <server_ip> <server_port> <topic> <output_dir> <num_connections>

# 3) Adaptive Parallel Transfer (--auto mode: benchmarks 1, 2, 4, 8 conns and selects best):
./Part2_Case2_Enhanced/client <server_ip> <server_port> <topic> <output_dir> --auto

# 4) Standalone Verification (--verify mode: verifies local downloaded files against server SHA-256):
./Part2_Case2_Enhanced/client <server_ip> <server_port> <topic> <output_dir> --verify
```

## Advanced Viva Features (Part 2 Case 2 Enhanced)

### 1. SHA-256 Application-Level File Integrity Verification
- **Why TCP Reliability is Insufficient:** TCP guarantees ordered, error-free byte streams *over the wire* via 16-bit checksums, but cannot detect data corrupted in kernel buffers, truncated disk writes, silent filesystem bit-rot, or application-level assembly bugs.
- **Cryptographic Digest Pipeline:**
  - **Server:** During session initialization (`MSG_SESSION_START`), computes SHA-256 digests of all source topic files using OpenSSL EVP streaming in bounded 64 KiB chunks (`common/sha256_util.c`). Binary digests (32 bytes) are embedded in `EnhancedFileMetadata.sha256`.
  - **Client Staging Isolation:** Incoming chunks are written concurrently to temporary `.part` files (`<output_dir>/<topic>/<file>.part`) via POSIX `pwrite()`.
  - **Verification Gate:** Once all work-units complete, the client hashes each staged `.part` file.
    - If digest matches: prints `SHA-256 verification: PASS (file=<name>, digest=<hex>)` and atomically publishes the file via `rename()`.
    - If digest mismatches: prints `SHA-256 verification: FAIL — file content does not match the expected digest` and removes the corrupt `.part` file (`unlink()`). Corrupted files are never published as completed.
- **Standalone Re-Verification (`--verify`):** Connects to server to fetch authoritative metadata and independently re-verifies completed local files without redownloading. Returns exit code 0 on PASS, 1 on FAIL.

### 2. Adaptive Parallel TCP Transfer (`--auto`)
- **Motivation:** Parallel TCP streams mitigate TCP slow-start and single-stream bottlenecking under packet loss, but excess connections induce thread contention, socket buffer exhaustion, and context-switch overhead.
- **Automated Performance Selection:**
  - Evaluates candidate connection counts: `C in {1, 2, 4, 8}`.
  - Runs clean, isolated trials in `.trial_<C>` directories to prevent caching artifacts.
  - Measures useful transfer duration via high-resolution monotonic clock (`clock_gettime(CLOCK_MONOTONIC)`).
  - Throughput calculation is strictly based on verified file payload bytes ($B_{\text{payload}} / T_{\text{elapsed}}$ in MiB/s and MB/s), excluding framing overhead.
  - **Deterministic Tie-Breaking:** If candidate throughput is within a 2% margin of the maximum measured throughput, the candidate with *fewer* connections is selected to minimize operating system resource utilization.
  - Automatically promotes winning trial files to the final destination directory and cleans up temporary trial workspaces.

### 3. Dependencies & Prerequisites
- **OpenSSL Cryptographic Library:**
  - `Part2_Case2_Enhanced/` links with OpenSSL's `libcrypto` (`-lcrypto`).
  - To install on Debian/Ubuntu/WSL:
    ```bash
    sudo apt-get update && sudo apt-get install -y libssl-dev
    ```

### 4. Viva Demonstration & Test Suite
Run the automated end-to-end demonstration harness:
```bash
bash tests/test_viva_features.sh
```
This harness executes:
1. Explicit 1-connection transfer and SHA-256 PASS verification.
2. Explicit 4-connection parallel transfer with concurrent `pwrite()` and SHA-256 PASS.
3. **Mandatory 1-Byte Corruption Test:** Flips a single byte in a completed local file, re-runs `--verify`, confirms explicit FAIL detection (exit code 1), and validates that the server source file is untouched.
4. Parallel transfer under probabilistic fault injection ($p=0.10$) with automatic recovery and SHA-256 PASS.
5. Adaptive Parallel Transfer (`--auto`), measuring throughput across 1, 2, 4, 8 connections and selecting the optimal configuration.
6. Command-line argument boundary validation (rejecting invalid counts such as 0 or 32).


## Fault-Injection Mechanism Used for Testing

All Part 2 servers (`Part2_Case1/server`, `Part2_Case2/server`, and `Part2_Case2_Enhanced/server`) link against `common/fault_injection.c`:
1. **Configuration:** At startup, `parse_failure_probability()` validates that `<failure probability>` is a valid floating-point number $p \in [0.0, 1.0]$.
2. **Thread-Safe Per-Chunk Decision:** Immediately before transmitting each 64 KiB file data chunk (`MSG_FILE_DATA` or `MSG_WORK_DATA`), the worker thread invokes `should_inject_fault()`. Each worker thread maintains a thread-local seed (`__thread unsigned int tls_seed`) and calls `rand_r(&tls_seed)` so concurrent threads never contend on global PRNG locks.
3. **Abrupt Socket Termination:** If the random draw is less than $p$, `execute_fault_disconnect(socket_fd, context_msg)` logs the exact timestamp, socket descriptor, active filename, and byte offset to `stderr` (`[FAULT] ...`) and immediately closes `socket_fd` via `close()` without transmitting any closing protocol frame.

## Running Automated Regression Tests, Benchmarks & Packaging

```bash
# 1. Run the 22-check automated correctness & diff verification suite across all 4 variants
bash scripts/test_correctness.sh

# 2. Execute the 4 benchmark experiments (generates Results/exp1..exp4_*.csv)
python3 scripts/run_benchmarks.py

# 3. Render the 4 publication-grade Matplotlib PNG graphs in Results/
python3 scripts/plot_results.py

# 4. Build and verify FinalSubmission_Group_071_072.tar.gz in a clean /tmp sandbox
bash scripts/package_submission.sh
```
