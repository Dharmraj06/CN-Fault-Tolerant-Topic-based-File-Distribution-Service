# TESTING & ACCEPTANCE SPECIFICATION
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Document**: Quality Assurance Test Matrix, Edge Cases & Verification Criteria  
**Target Environment**: Linux / GCC / POSIX Sockets  

---

## 1. Quality Assurance Strategy

Testing is split into two rigorous tiers:
1. **Automated Verification Harness (`scripts/test_all_correctness.sh`)**: Executed locally and in CI to guarantee zero regressions across builds.
2. **Viva Demonstration Suite**: Scripted procedures prepared specifically for presentation to course instructors and teaching assistants.

---

## 2. Master Test Matrix

---

### Category A: Build & Command-Line Interface Contracts

| Test ID | Test Name | Purpose | Command / Procedure | Expected Output | Pass/Fail Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TEST-BLD-01` | Independent Folder Build | Verify `make` works inside each directory | `cd Part1 && make clean && make`<br>`cd ../Part2_Case1 && make clean && make`<br>`cd ../Part2_Case2 && make clean && make`<br>`cd ../Part2_Case2_Enhanced && make clean && make` | Exact executables `server` and `client` created in each directory. | Pass: 0 compiler errors, 0 warnings with `-Wall -Wextra`. |
| `TEST-CLI-01` | Part 1 Server Arguments | Validate `./server <port> <root>` | `./Part1/server` (no args)<br>`./Part1/server abc /tmp`<br>`./Part1/server 999999 /tmp` | Prints usage message to stderr, exits with non-zero status. | Pass: Exits with status > 0 without segmentation fault. |
| `TEST-CLI-02` | Part 2 Server Arguments | Validate `./server <port> <root> <prob>` | `./Part2_Case1/server 8080 /tmp -0.5`<br>`./Part2_Case1/server 8080 /tmp 1.5`<br>`./Part2_Case1/server 8080 /tmp NaN` | Rejects probabilities outside `[0.0, 1.0]`. | Pass: Rejects invalid probability, exits non-zero. |
| `TEST-CLI-03` | Client Arguments | Validate `./client <ip> <port> <topic> <out>` | `./Part1/client 127.0.0.1 8080` (missing args)<br>`./Part1/client 999.999.999.999 8080 cane /tmp` | Prints client usage or connection error, exits non-zero. | Pass: Clean exit code > 0. |

---

### Category B: Data Transfer Correctness & File Integrity

| Test ID | Test Name | Purpose | Command / Procedure | Expected Output | Pass/Fail Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TEST-COR-01` | Standard Image Topic Transfer | Verify transfer of multiple image files | Start server on `animals10` dataset.<br>`./client 127.0.0.1 8080 cane ./output`<br>`diff -r <root>/cane ./output/cane` | All JPG/PNG files transferred. `diff -r` returns empty. | Pass: `diff -r` exit code 0; SHA256 checksums match 100%. |
| `TEST-COR-02` | Empty Topic Directory | Verify handling of topic with 0 files | Create empty dir `<root>/empty_topic`.<br>`./client 127.0.0.1 8080 empty_topic ./output` | Creates `./output/empty_topic/`, transfers 0 files, succeeds. | Pass: Exit code 0, empty directory created. |
| `TEST-COR-03` | Zero-Byte Empty File | Verify transfer of 0-byte file | Place empty file `zero.txt` in topic.<br>Run client. | `zero.txt` created in destination with size 0 bytes. | Pass: `stat -c %s zero.txt` returns 0; no hanging sockets. |
| `TEST-COR-04` | Large Binary File (50MB+) | Verify streaming chunking and memory bounds | Generate 50MB random binary file `head -c 50M /dev/urandom > file.bin`. Run client. | Server memory usage remains < 10MB during transfer. `diff` matches. | Pass: Zero RAM exhaustion, exact byte match. |
| `TEST-COR-05` | Non-Existent Topic | Verify server error on missing topic | `./client 127.0.0.1 8080 non_existent_topic ./output` | Server sends `ERR_TOPIC_NOT_FOUND`; client logs error and exits. | Pass: Client exits with status code > 0. |
| `TEST-COR-06` | Path Traversal Security | Verify rejection of directory escapes | `./client 127.0.0.1 8080 "../../etc" ./output`<br>`./client 127.0.0.1 8080 "cane/sub" ./output` | Server rejects request before touching filesystem. | Pass: Request rejected; zero filesystem access outside root. |

---

### Category C: Multi-Threaded Concurrency

| Test ID | Test Name | Purpose | Command / Procedure | Expected Output | Pass/Fail Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TEST-CON-01` | Simultaneous Same Topic | 4 clients request same topic concurrently | Launch 4 client processes in parallel downloading `cavallo`. | All 4 clients complete successfully without cross-talk or corruption. | Pass: All 4 client folders match source 100%. |
| `TEST-CON-02` | Simultaneous Different Topics | 4 clients request 4 distinct topics | Client 1: `cane`, Client 2: `gatto`, Client 3: `mucca`, Client 4: `pecora`. | Worker threads process independent topics simultaneously. | Pass: All outputs verify cleanly. |
| `TEST-CON-03` | Sequential Head-of-Line Blocking | Verify Part 1 sequential server blocks | Launch slow client with throttled socket; launch second client. | Second client waits in backlog until first client finishes. | Pass: Verifies sequential behavior required for Part 1 baseline. |

---

### Category D: Fault Injection & Recovery (Part 2)

| Test ID | Test Name | Purpose | Command / Procedure | Expected Output | Pass/Fail Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TEST-FLT-01` | Zero Failure Baseline (`prob=0.0`) | Verify Part 2 behaves like Part 1 when no faults | Run `Part2_Case1` and `Case2` with `prob=0.0`. | Zero connection drops; transfer completes on first attempt. | Pass: 1 attempt, 0 retries. |
| `TEST-FLT-02` | Case 1 Full Topic Restart | Verify Case 1 discards progress on disconnect | Server `prob=0.15`. Client downloads `cane`.<br>Monitor server logs. | On disconnect, client cleans partial output, reconnects, requests file 0. | Pass: All files intact at end; redundant bytes > 0. |
| `TEST-FLT-03` | Case 2 Checkpoint Resume | Verify Case 2 resumes from byte offset | Server `prob=0.15`. Client downloads `cane`.<br>Monitor client logs. | On disconnect, client reconnects with `session_id`, resumes from last chunk. | Pass: Resumed byte offset matches last ACK; redundant bytes near 0. |
| `TEST-FLT-04` | Guaranteed Failure (`prob=1.0`) with Retry Bound | Verify bounded retries on permanent failure | Server `prob=1.0`. Client downloads topic. | Client retries up to maximum retry limit (e.g. 5), then aborts cleanly. | Pass: Client does not loop infinitely; exits non-zero after max retries. |
| `TEST-FLT-05` | Case 2 Expired Session Handling | Verify server response to stale session | Client connects, pauses for 310s (exceeding TTL), resumes. | Server returns `ERR_SESSION_EXPIRED`; client starts fresh transfer. | Pass: Graceful handling without crash. |

---

### Category E: Case 2 Enhanced Parallel TCP Transfer

| Test ID | Test Name | Purpose | Command / Procedure | Expected Output | Pass/Fail Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TEST-ENH-01` | Default Invocation (1 Connection) | Verify standard 4-argument CLI works | `./Part2_Case2_Enhanced/client 127.0.0.1 8083 cane ./output` | Transfer succeeds using 1 connection. | Pass: Satisfies automated grading test harness. |
| `TEST-ENH-02` | Multi-Connection Transfer (4 Connections) | Verify parallel transfer across 4 TCP sockets | `./Part2_Case2_Enhanced/client 127.0.0.1 8083 cane ./output 4` | 4 sockets open; files downloaded in parallel; `pwrite` reassembles. | Pass: `diff -r` clean; completion time faster than 1 connection. |
| `TEST-ENH-03` | Single Worker Connection Failure | Verify failure on 1 socket does not abort other sockets | Inject fault on socket 2 while sockets 1, 3, 4 are transferring. | Socket 2 reconnects and resumes its work unit; sockets 1, 3, 4 continue. | Pass: All work units complete; final dataset verified 100%. |

---

## 3. Automated Test Script Specification

The script `scripts/test_all_correctness.sh` will execute all above tests sequentially:

```bash
#!/usr/bin/env bash
# scripts/test_all_correctness.sh
set -euo pipefail

echo "======================================================"
echo "RUNNING FULL AUTOMATED REGRESSION & EDGE-CASE TEST SUITE"
echo "======================================================"

# 1. Build Verification
for dir in Part1 Part2_Case1 Part2_Case2 Part2_Case2_Enhanced; do
    echo "[BUILD] Checking $dir..."
    (cd "$dir" && make clean && make -j4)
done

# 2. Dataset Generation
./scripts/generate_test_dataset.sh /tmp/cn_test_topics

# 3. Part 1 Sequential & Concurrent Tests
# ... Start server, run clients, run diff -r ...

# 4. Part 2 Case 1 Fault & Restart Tests
# ... Verify restart from file 0 ...

# 5. Part 2 Case 2 Checkpoint Resume Tests
# ... Verify resume from byte offset ...

# 6. Part 2 Case 2 Enhanced Parallel Tests
# ... Verify 1 and 4 connections ...

echo "======================================================"
echo "ALL TESTS PASSED WITH 100% SUCCESS RATE!"
echo "======================================================"
```
