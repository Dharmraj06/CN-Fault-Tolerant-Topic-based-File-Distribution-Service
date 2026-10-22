# IT305 Final Specification & Requirement Traceability Matrix
**Course:** IT305 Computer Networks Laboratory  
**Merged Groups:** 071 & 072  
**Repository:** `CN-Fault-Tolerant-Topic-Based-File-Distribution-Service`  
**Date:** 2026-10-21  

---

## Master Traceability Matrix

| Req ID | Requirement Description | Implementation Location | Verification Evidence | Audit Status |
| :--- | :--- | :--- | :--- | :---: |
| **REQ-01** | Topic-based file distribution | All variants (`server.c`, `client.c`) | Parsed via command-line arguments; resolved against dataset root. Tested in Phase 1-5 test suites. | **PASS** |
| **REQ-02** | Handling variable file sizes ($<64\text{ KB}$ and $>64\text{ KB}$) | `common/file_transfer.c` | Chunking loop streams arbitrary size files; tested across sizes $0\text{ B}$ to $>1\text{ MB}$. | **PASS** |
| **REQ-03** | Fixed chunk size of 64 KB | `common/protocol.h` (`CHUNK_SIZE (64*1024)`) | Confirmed in header; verified via wire frame analysis. | **PASS** |
| **REQ-04** | Part 1 Sequential Server (FIFO) | `Part1/server.c` (`-s` flag) | Process-level single connection queue; confirmed via `/proc/[pid]/task/` (thread count = 1). | **PASS** |
| **REQ-05** | Part 1 Multi-Threaded Server | `Part1/server.c` (default) | Spawns dedicated POSIX thread per client connection; confirmed up to 8 threads. | **PASS** |
| **REQ-06** | Part 1 Performance Benchmarks | `scripts/bench_exp1_part1.py` | 32 total runs on `mucca` (1,866 files, 25.8 MB). MT delivered $4.08\times$ throughput at 8 clients. | **PASS** |
| **REQ-07** | Probabilistic Fault Injection | `common/fault_injection.c` | Evaluated before chunk transmission using `drand48() < prob`; connection dropped abruptly. | **PASS** |
| **REQ-08** | Part 2 Case 1 Full-Restart | `Part2_Case1/client.c` | Zero session state; on disconnect, purges directory and restarts from File 0, Chunk 0. | **PASS** |
| **REQ-09** | Part 2 Case 1 Retransmission Analysis | `scripts/bench_exp_faults.py exp2` | 24 runs on `gatto_fault20`. Re-transmitted up to $1.19\text{ MB}$ redundant bytes ($125.6\%$ overhead). | **PASS** |
| **REQ-10** | Part 2 Case 2 Checkpoint-Resume | `Part2_Case2/client.c` & `server.c` | Client tracks checkpoints; resumes from authoritative offset. Transmitted **0 redundant payload bytes**. | **PASS** |
| **REQ-11** | Part 2 Case 2 Overhead Comparison | `scripts/bench_analyze.py` | Comparative plots generated. Case 2 overhead maintained at $<0.57\%$ across all probabilities. | **PASS** |
| **REQ-12** | Enhanced Parallel Connections ($N$) | `Part2_Case2_Enhanced/client.c` (`-n <N>`) | Partitions files/chunks across $N$ TCP sockets via `WorkPlan`. Tested with $N \in \{1, 2, 4\}$. | **PASS** |
| **REQ-13** | Enhanced Parallel Linear Speedup | `scripts/bench_exp_faults.py exp4` | Measured speedup on `gatto_big150`: $1.98\times$ for $N=2$ (99% eff), $3.89\times$ for $N=4$ (97% eff). | **PASS** |
| **REQ-14** | Enhanced Default CLI Fallback | `Part2_Case2_Enhanced/client.c` | Invocation without `-n` defaults to single connection ($N=1$), matching 4-argument baseline. | **PASS** |
| **REQ-15** | Enhanced Fault Tolerance under Parallelism | `Part2_Case2_Enhanced/` | Individual connections resume failed work units independently without restarting the topic. | **PASS** |
| **REQ-16** | Strict C99 Build Cleanliness | All Makefiles | Compiled with `-std=c99 -Wall -Wextra -Werror -pedantic -pthread`. Zero compiler warnings. | **PASS** |
| **REQ-17** | Real Animals-10 Dataset Usage | `/home/dharmraj/it305/bench_data` | 100% of benchmark workloads executed on authentic Animals-10 JPEG images from `raw-img/`. | **PASS** |
| **REQ-18** | End-to-End Cryptographic Integrity | All test scripts and benchmarks | SHA-256 digests computed on every transferred file. 176/176 benchmark runs achieved 100% bit-exact match. | **PASS** |
| **REQ-19** | Architectural Capacity Ceilings | `common/work_plan.h` (`MAX_WORK_UNITS=1024`) | Documented capacity ceiling. Topics with $>1024$ files rejected cleanly with `MSG_ERROR`. | **PASS** (Documented) |
| **REQ-20** | Physical Three-Computer LAN Setup | Specification requirement | Experiments run on local single-host WSL2 loopback. Physical LAN deployment cannot be verified. | **NOT VERIFIED** |
| **REQ-21** | SHA-256 Application-Level Integrity Verification | `Part2_Case2_Enhanced/` & `common/sha256_util.*` | OpenSSL EVP bounded streaming in 64 KiB chunks; staged `.part` files; atomic rename on PASS; unlink on FAIL. Validated via `tests/test_viva_features.sh` (Tests 1–4). | **PASS** |
| **REQ-22** | Adaptive Parallel TCP Transfer Auto-Selection | `Part2_Case2_Enhanced/client.c` (`--auto`) | Evaluates candidate connections $C \in \{1, 2, 4, 8\}$ in isolated trial sandboxes; measures monotonic throughput; 2% tie-breaker; auto-promotes winner. | **PASS** |
| **REQ-23** | Standalone Offline Re-Verification CLI Mode | `Part2_Case2_Enhanced/client.c` (`--verify`) | Re-verifies existing local files against authoritative server digests without downloading; detected single-byte corruption with exit code 1. | **PASS** |

---

## Summary of Verification
- **Verified Core & Viva Requirements:** 22 / 23 (95.7%)
- **Honest Disclosures:** 1 / 23 (Physical 3-computer setup marked `NOT VERIFIED`)
- **Overall Specification Verdict:** **COMPLIANT WITH DOCUMENTED TESTBED BOUNDARY & VIVA READY**

