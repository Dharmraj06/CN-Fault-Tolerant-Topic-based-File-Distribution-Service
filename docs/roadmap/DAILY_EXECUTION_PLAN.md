# DAILY EXECUTION PLAN: DAY-BY-DAY TEAM SCHEDULE (12–16 OCTOBER 2026)
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Execution Horizon**: Monday 12 October 2026 – Friday 16 October 2026 (23:59 IST)  

---

## 1. Schedule Overview & Daily Goals

```
  12-13 Oct (Day 1)          14 Oct (Day 2)             15 Oct (Day 3)             16 Oct (Day 4)
┌──────────────────────┐   ┌──────────────────────┐   ┌──────────────────────┐   ┌──────────────────────┐
│  Phase 1 & Phase 2   │   │  Phase 4 & Phase 5   │   │  Phase 6 & Phase 7   │   │  Phase 8 & Final     │
│  - Protocol Freeze   │   │  - Fault Injection   │   │  - Enhanced Parallel │   │  - Benchmarks / Data │
│  - Shared Modules    │──►│  - Case 1 Restart    │──►│  - System Regression │──►│  - Graph Generation  │
│  - Part 1 Seq & MT   │   │  - Case 2 Session    │   │  - Multi-Node Stress │   │  - Final Report PDF  │
│  - Part 1 Verified   │   │  - Case 2 Resume     │   │  - Code Freeze       │   │  - Final Archive Zip │
└──────────────────────┘   └──────────────────────┘   └──────────────────────┘   └──────────────────────┘
```

---

## 2. Daily Member Schedule

---

### Day 1: Tuesday, 13 October 2026
**Theme**: *Shared Foundation & Part 1 Delivery*  
**Daily Milestone M4 Target**: Part 1 builds and delivers topic files sequentially and concurrently with 100% data fidelity.

#### Morning (09:00 – 13:00 IST)
- **Dharmraj Vaghela**: Complete `TSK-P01-01`. Push initial commit containing `docs/roadmap/` to GitHub `main`. Create branch `develop`. Set up team branch rules.
- **Jay Vaghela**: Complete `TSK-P02-01`. Write `common/protocol.h` and `common/protocol.c` (`send_all`, `recv_all`, binary framing).
- **Vivek Dodiya**: Complete `TSK-P02-03`. Write `common/file_transfer.h` and `common/file_transfer.c` (chunked streaming, directory creation).
- **Shlok Topaliya**: Complete `TSK-P02-04`. Implement `common/logging.h` and `common/logging.c` (monotonic clock, thread-safe logger).
- **Atharv Trivedi**: Set up local test environments and study fault injection random number generators.
- **Yug Malaviya**: Prepare `scripts/generate_test_dataset.sh` generating sample topic directory trees with varying file sizes.

#### Afternoon (14:00 – 18:00 IST)
- **Jay Vaghela**: Complete `TSK-P02-02`. Implement `common/topic_manager.c` (directory scanner, `qsort` manifest, path traversal filter).
- **Vivek Dodiya**: Begin `TSK-P03-03`. Implement `Part1/client.c` (argument parsing, socket connection, receiving manifest).
- **Dharmraj Vaghela**: Review Jay's `protocol.c` and Vivek's `file_transfer.c`. Merge to `develop`.
- **Atharv Trivedi**: Begin `TSK-P04-01`. Implement `common/fault_injection.c`.
- **Shlok Topaliya**: Unit test logging with concurrent worker threads.
- **Yug Malaviya**: Create automated build check script for `Part1/Makefile`.

#### Evening (19:00 – 23:00 IST)
- **Jay Vaghela**: Complete `TSK-P03-01` and `TSK-P03-02`. Implement `Part1/server.c` (sequential mode and multi-threaded `pthread_create` mode).
- **Vivek Dodiya**: Finalize `Part1/client.c`. Build `Part1/Makefile`.
- **Atharv Trivedi & Dharmraj Vaghela**: Conduct loopback integration tests between `Part1/server` and `Part1/client`.
- **Yug Malaviya**: Run `diff -r` test across all transferred topics.

#### End-of-Day Checkpoint (23:30 IST)
```bash
# Verify Part 1 builds and runs cleanly
cd Part1 && make clean && make
./server 8080 ../test_topics &
./client 127.0.0.1 8080 cane ./output
diff -r ../test_topics/cane ./output/cane
# Expected: Exit code 0, zero differences.
```

---

### Day 2: Wednesday, 14 October 2026
**Theme**: *Fault Injection, Case 1 Full Restart & Case 2 Checkpoint Resume*  
**Daily Milestone M7 Target**: Both Part 2 Case 1 and Part 2 Case 2 build, survive injected disconnects, and recover with zero corrupted files.

#### Morning (09:00 – 13:00 IST)
- **Atharv Trivedi**: Complete `TSK-P04-01` and `TSK-P04-02`. Integrate fault injection into `Part2_Case1/server.c`.
- **Vivek Dodiya**: Complete `TSK-P04-03`. Implement `Part2_Case1/client.c` (abrupt close detection, purge directory, reconnect loop).
- **Dharmraj Vaghela**: Complete `TSK-P05-01`. Implement `common/session.c` (mutex-protected session table, 64-bit ID generation, checkpoint commit logic).
- **Jay Vaghela**: Peer review `Part2_Case1` PRs. Implement unit test harness for session table concurrency (`test_session.c`).
- **Shlok Topaliya**: Begin `TSK-P06-01`. Research and scaffold `common/work_plan.c`.
- **Yug Malaviya**: Write automated test runner for Case 1 restart behavior.

#### Afternoon (14:00 – 18:00 IST)
- **Atharv Trivedi & Vivek Dodiya**: Integrate and test `Part2_Case1`. Verify that client recovers from server connection drops at `prob=0.10` and restarts from file 0.
- **Dharmraj Vaghela**: Complete `TSK-P05-02`. Implement `Part2_Case2/server.c` (session lookup, `CHUNK_ACK` handling, authoritative resume offset validation).
- **Atharv Trivedi**: Begin `TSK-P05-03`. Implement `Part2_Case2/client.c` (checkpoint tracking, sending `CHUNK_ACK`, resume handshake).
- **Jay Vaghela**: Support session resume verification logic and error handling.
- **Shlok Topaliya**: Implement work unit range slicing in `work_plan.c`.

#### Evening (19:00 – 23:00 IST)
- **Dharmraj Vaghela & Atharv Trivedi**: Connect `Part2_Case2/server` and `Part2_Case2/client`. Perform fault injection tests.
- **Vivek Dodiya**: Verify that client in Case 2 successfully seeks to existing file offset upon resume and does not duplicate bytes.
- **Yug Malaviya**: Verify byte counters: confirm that Case 2 sends significantly fewer redundant bytes than Case 1.
- **Shlok Topaliya**: Finalize `work_plan.c` unit tests.

#### End-of-Day Checkpoint (23:30 IST)
```bash
# Verify Case 1 and Case 2 build and recover cleanly
cd Part2_Case1 && make clean && make
./server 8081 ../test_topics 0.15 &
./client 127.0.0.1 8081 cane ./output_c1
diff -r ../test_topics/cane ./output_c1/cane

cd ../Part2_Case2 && make clean && make
./server 8082 ../test_topics 0.15 &
./client 127.0.0.1 8082 cane ./output_c2
diff -r ../test_topics/cane ./output_c2/cane
# Expected: Exit code 0 for both cases.
```

---

### Day 3: Thursday, 15 October 2026
**Theme**: *Case 2 Enhanced Parallelism & Full System Integration*  
**Daily Milestone M9 Target**: All 4 directories fully implemented, all edge cases handled, code frozen for benchmarking.

#### Morning (09:00 – 13:00 IST)
- **Shlok Topaliya**: Complete `TSK-P06-01` and `TSK-P06-02`. Implement `Part2_Case2_Enhanced/server.c` and `client.c` (multi-connection work unit allocation, `pwrite()` file writing).
- **Vivek Dodiya**: Test `Part2_Case2_Enhanced/client.c` with default 1 connection (confirming standard invocation compatibility).
- **Jay Vaghela**: Review `Part2_Case2_Enhanced` code. Verify thread safety of destination file writes.
- **Dharmraj Vaghela**: Audit session state handling across multiple simultaneous connections belonging to one session.
- **Atharv Trivedi**: Test connection failure on 1 of 4 parallel workers (verify that only the failed work unit is retried).
- **Yug Malaviya**: Build `scripts/test_all_correctness.sh` covering all edge cases.

#### Afternoon (14:00 – 18:00 IST)
- **Shlok Topaliya & Vivek Dodiya**: Stress-test `Part2_Case2_Enhanced` with 1, 2, 4, and 8 parallel connections.
- **Yug Malaviya**: Run edge-case matrix:
  - Empty topic directory
  - Empty (0-byte) files
  - Large binary files (50 MB+)
  - Invalid topic names and path traversal attempts (`../../`)
  - Server shutdown during transfer
- **Jay Vaghela & Dharmraj Vaghela**: Fix any protocol or edge-case bugs identified by the test runner.
- **Atharv Trivedi**: Verify exit codes: ensure client returns `0` on success and non-zero on unrecoverable failure.

#### Evening (19:00 – 23:00 IST)
- **Team Code Freeze**: Merge all feature branches into `develop`. Merge `develop` into `main`.
- **Yug Malaviya**: Complete `TSK-P07-02`. Finalize `scripts/run_benchmarks.py`.
- **Shlok Topaliya**: Complete `TSK-P08-01`. Write `scripts/plot_results.py`.
- **All Members**: Execute trial benchmark run to ensure timing and CSV logging produce clean numbers.

#### End-of-Day Checkpoint (23:30 IST)
```bash
# Run comprehensive automated test harness across all four parts
./scripts/test_all_correctness.sh
# Expected: 100% tests PASSED across Part1, Part2_Case1, Part2_Case2, Part2_Case2_Enhanced.
```

---

### Day 4: Friday, 16 October 2026 (Deadline Day)
**Theme**: *Performance Benchmarking, Graphs, Final Report & Submission Packaging*  
**Daily Milestone M12 Target**: Complete submission archive verified and uploaded by 21:00 IST (3 hours ahead of deadline).

#### Morning (09:00 – 13:00 IST)
- **Yug Malaviya & Shlok Topaliya**: Execute official performance benchmarks on the 3-computer lab setup (or dedicated loopback environment):
  - Part 1: Sequential vs Multi-Threaded across 1, 2, 4, 8 concurrent clients.
  - Part 2: Case 1 vs Case 2 across failure probabilities 0.0, 0.01, 0.05, 0.10.
  - Case 2 vs Enhanced: 1 connection vs 2, 4 parallel connections.
- **Jay Vaghela & Dharmraj Vaghela**: Monitor data collection, verify raw CSV numbers in `Results/`, ensure no anomalies or packet drops outside configured faults.
- **Vivek Dodiya & Atharv Trivedi**: Begin drafting the technical narrative for the Final Report.

#### Afternoon (13:00 – 17:00 IST)
- **Shlok Topaliya**: Run `scripts/plot_results.py` on the fresh CSV data. Generate all required PNG graphs in `Results/`:
  - `Part1_throughput.png`
  - `Case1_vs_Case2_throughput.png`
  - `Case1_vs_Case2_overhead.png`
  - `Enhanced_speedup.png`
- **Yug Malaviya**: Complete `TSK-P08-02`. Assemble `FinalReport_Group_071_072.pdf`:
  - Cover page: Group 071 + Group 072, all 6 names & roll numbers, 1-line contribution note each.
  - Architecture and implementation details for each variant.
  - Documented deviations from the submitted design document.
  - Embedded performance tables, graphs, and comparative analysis.

#### Evening (17:00 – 21:00 IST) — Final Verification & Release
- **Dharmraj Vaghela**: Complete `TSK-P08-03`. Generate the consolidated root `README.md`. Run `scripts/package_submission.sh` to produce `FinalSubmission_Group_071_072.zip`.
- **All Team Members**: Extract the zip archive in a clean directory on a separate machine:
  - Run `make` in each of the 4 directories.
  - Test CLI invocations with grader-style arguments.
  - Verify PDF report formatting.
- **21:00 IST**: Submit `FinalSubmission_Group_071_072.zip` to the course portal.
- **21:00 – 23:59 IST**: Buffer window for submission confirmation and contingency.
