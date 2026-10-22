# Advanced Viva Features: Verification & Benchmark Report

**Project:** Fault-Tolerant Topic-Based File Distribution Service  
**Course:** IT305 Computer Networks Lab (Autumn 2026), DA-IICT  
**Merged Group:** Group 071 + Group 072  
**Target Variant:** `Part2_Case2_Enhanced/` (Multi-Socket Parallel TCP Transfer)  
**Date:** 2026-10-22  
**Testbed Environment:** Linux 6.6.x (WSL2 Ubuntu 24.04), GCC 15.2.0, OpenSSL 3.5.5 (`libcrypto`), Loopback Interface (`127.0.0.1`)

---

## 1. Executive Summary

This report provides the formal specification, experimental results, and architectural verification for the two viva demonstration features implemented in `Part2_Case2_Enhanced/`:
1. **Feature One:** Cryptographic SHA-256 Application-Level File Integrity Verification & Staged Publication.
2. **Feature Two:** Adaptive Parallel TCP Transfer Auto-Selection (`--auto`).

Both features have been built under strict C99 standards (`-std=c99 -Wall -Wextra -Werror -pedantic -pthread`), validated against the 22-check master regression harness, and demonstrated via `tests/test_viva_features.sh` (6/6 tests passing, 0 failures).

---

## 2. Feature One: SHA-256 Application-Level File Integrity Verification

### 2.1 The End-to-End Integrity Problem in Computer Networks
TCP provides reliable, in-order byte stream delivery over individual network connections via 16-bit Internet checksums (RFC 793, RFC 9293). However, in distributed systems:
- TCP checksums only detect network packet bit-flips in transit between network interface cards; they do not detect data corruption occurring prior to packet transmission (e.g., host memory bit flips, buggy concurrent writers) or after packet reception (e.g., incomplete filesystem writes, disk sector corruption).
- In multi-stream parallel transfer where files are partitioned into arbitrary chunks written concurrently via `pwrite()`, synchronization errors or dropped chunks can lead to missing byte ranges or zero-filled holes.
- End-to-end file integrity verification requires an application-layer cryptographic digest calculated over the complete source file and compared against the finalized destination file.

### 2.2 Architectural Implementation
- **Cryptographic Engine:** Implemented in `common/sha256_util.c` using OpenSSL EVP (`EVP_DigestInit_ex`, `EVP_DigestUpdate`, `EVP_DigestFinal_ex`). File reading is performed in streaming chunks of 64 KiB, ensuring strict $O(1)$ memory consumption regardless of file size.
- **Protocol Extension:** `Part2_Case2_Enhanced/enhanced_protocol.h` incorporates `uint8_t sha256[32]` into `EnhancedFileMetadata`. The frozen baseline protocol `common/protocol.h` remains completely untouched.
- **Server Operation:** During session initialization upon `MSG_SESSION_START`, the server streams each file from disk, computes its SHA-256 digest, and embeds it in the manifest.
- **Client Staging Isolation:** The client downloads incoming data into temporary `.part` files (e.g., `filename.ext.part`).
- **Atomic Publication Gate:**
  - Upon receiving all allocated chunks, the client computes the SHA-256 digest of each `.part` file.
  - If the computed digest matches the server's expected digest:
    `SHA-256 verification: PASS (file=<name>, digest=<hex>)`
    The file is atomically promoted to its final name using POSIX `rename()`.
  - If any byte mismatches:
    `SHA-256 verification: FAIL — file content does not match the expected digest`
    The corrupted `.part` file is deleted via `unlink()`. No corrupted file is ever published as valid.
- **Standalone Offline Re-Verification:** `./Part2_Case2_Enhanced/client <ip> <port> <topic> <out_dir> --verify` fetches authoritative metadata from the server and independently verifies all existing local files without downloading.

### 2.3 Mandatory 1-Byte Corruption Demonstration
In `tests/test_viva_features.sh` (Test 3):
1. A valid file (`small.txt`, 78 bytes) was transferred and verified PASS.
2. Exactly one byte at offset 10 was flipped from `'N'` to `'X'` (`"Computer Networks"` -> `"Computer Xetworks"`), keeping file size strictly at 78 bytes.
3. Verification was re-executed.
4. **Result:** The corruption was detected immediately:
   - Expected: `80dcb22236a56d09574340af168fd0f5a84d04d790db1b1d17c16e47f4dd002c`
   - Actual:   `55fc306fa0761e053a63ec5164f9bfdfcae235a9f993d0774a38dfc6f1dc7855`
   - Status: `SHA-256 verification: FAIL` (Exit code: 1).
   - Server source file SHA-256 was checked and confirmed 100% untouched.

---

## 3. Feature Two: Adaptive Parallel TCP Transfer (`--auto`)

### 3.1 Motivation & Network Dynamics
Parallel TCP transfers divide a workload across multiple sockets, mitigating the impact of single-connection TCP slow-start and packet loss recovery bottlenecks. However, unbounded connection scaling causes:
- Thread contention and CPU context switching.
- Kernel socket buffer exhaustion.
- Diminishing returns in throughput due to Amdahl's Law and connection teardown overheads.

The `--auto` mode allows the Enhanced client to autonomously benchmark candidate connection counts and select the configuration that delivers optimal useful throughput.

### 3.2 Trial Isolation & Benchmarking Protocol
- **Candidates:** $C \in \{1, 2, 4, 8\}$ parallel TCP connections (within the supported limit of `ENHANCED_MAX_CONNECTIONS = 16`).
- **Clean Sandbox Isolation:** Each candidate trial downloads into an isolated directory (`<output_dir>/.trial_<C>/`), ensuring trial $C_i$ never reuses cached files or partial progress from trial $C_{i-1}$.
- **High-Resolution Timing:** Transfer durations are measured using monotonic clock (`clock_gettime(CLOCK_MONOTONIC)`).
- **Useful Throughput Metric:** Calculated strictly on verified file payload bytes ($B_{\text{payload}}$), excluding protocol framing headers:
  $$\text{Throughput} = \frac{B_{\text{payload}}}{T_{\text{elapsed}}}$$
- **Tie-Breaking Rule:** If a candidate achieves throughput within 2% of the maximum measured throughput, the configuration with **fewer connections** is selected to conserve operating system socket and thread resources.
- **Atomic Promotion:** The winning configuration's verified files are moved to `<output_dir>/<topic>/`, and temporary sandbox directories are cleaned up.

### 3.3 Empirical Benchmark Results

Below are the actual measurements recorded from loopback benchmarking on topic `demo_topic` ($262,878$ payload bytes):

| Trial ID | Connections ($C$) | Verified Payload (Bytes) | Elapsed Time (s) | Useful Throughput (MiB/s) | Useful Throughput (MB/s) | Verification | Selection |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 1 | 262,878 | 0.3604 | 0.70 MiB/s | 0.73 MB/s | **PASS** | No |
| 2 | 2 | 262,878 | 0.1867 | 1.34 MiB/s | 1.41 MB/s | **PASS** | No |
| 3 | 4 | 262,878 | 0.1809 | **1.39 MiB/s** | **1.45 MB/s** | **PASS** | **SELECTED** |
| 4 | 8 | 262,878 | 0.1898 | 1.32 MiB/s | 1.38 MB/s | **PASS** | No |

#### Observations:
1. **Parallel Speedup:** Scaling from 1 connection to 4 connections increased throughput from $0.70\text{ MiB/s}$ to $1.39\text{ MiB/s}$ ($1.99\times$ throughput speedup, cutting transfer time from $0.3604\text{ s}$ to $0.1809\text{ s}$).
2. **Diminishing Returns & Overhead at 8 Connections:** At $C=8$, throughput dropped from $1.39\text{ MiB/s}$ down to $1.32\text{ MiB/s}$. The overhead of creating 8 concurrent sockets, assigning thread workloads, and coordinating teardown exceeded the concurrency benefit on this workload size.
3. **Selection Accuracy:** The adaptive transfer engine correctly identified $C=4$ as the peak throughput configuration.

---

## 4. Full Viva Demonstration Suite (`tests/test_viva_features.sh`)

| Test ID | Test Description | Condition Tested | Expected Outcome | Actual Result | Status |
| :---: | :--- | :--- | :--- | :--- | :---: |
| **VIVA-01** | Explicit 1-Connection Transfer | Baseline single stream | All files downloaded; SHA-256 PASS; diff clean | 100% bit-exact match | **PASS** |
| **VIVA-02** | Explicit 4-Connection Parallel Transfer | Concurrent `pwrite()` across 4 TCP sockets | All files assembled; SHA-256 PASS; diff clean | 100% bit-exact match | **PASS** |
| **VIVA-03** | Mandatory 1-Byte Corruption Test | 1 byte flipped at offset 10; size unchanged | Explicit FAIL message; exit code 1; server source unmodified | Corrupted file caught; code 1; server untouched | **PASS** |
| **VIVA-04** | Transfer Under Fault Injection ($p=0.10$) | Probabilistic drops during multi-socket stream | Workers reconnect & resume; SHA-256 PASS | Eventual bit-exact completion | **PASS** |
| **VIVA-05** | Adaptive Transfer Auto-Selection (`--auto`) | Automated evaluation of $C \in \{1, 2, 4, 8\}$ | Benchmarks each isolated trial; picks best; promotes files | Selected optimal $C=4$; bit-exact diff clean | **PASS** |
| **VIVA-06** | CLI Boundary & Error Validation | Invalid args: `0`, `32`, `--bogus` | Rejected safely with exit code 1; usage printed | Clean error handling | **PASS** |

---

## 5. Summary Verdict
Both viva features are fully implemented, verified, documented, and backward-compatible with all previous assignment deliverables.
