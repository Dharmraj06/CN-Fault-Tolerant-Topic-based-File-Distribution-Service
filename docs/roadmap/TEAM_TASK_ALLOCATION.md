# TEAM TASK ALLOCATION & WORKLOAD MATRIX
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Group**: Group 071 + Group 072 (6 Members)  
**Execution Window**: 12 October 2026 – 16 October 2026  

---

## 1. Workload Distribution Summary

To ensure fair contribution and meet the course's strict individual viva evaluation criteria, engineering duties are evenly distributed. No team member is assigned purely documentation or testing tasks; every student owns core C networking modules.

| Member Name | Student ID | Primary Focus Area | Secondary Focus Area | Assigned Task IDs | Est. Effort |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Jay Vaghela** | 202401237 | Protocol Wire Engine & Manifests | Part 1 Sequential & MT Servers | `TSK-P02-01`, `TSK-P02-02`, `TSK-P03-01`, `TSK-P03-02` | **13.5 hrs** |
| **Dharmraj Vaghela** | 202401236 | Session Management Subsystem | Part 2 Case 2 Server & Repo Lead | `TSK-P01-01`, `TSK-P05-01`, `TSK-P05-02`, `TSK-P08-03` | **15.0 hrs** |
| **Vivek Dodiya** | 202401253 | Bounded File I/O Engine | Part 1 & Case 1 Clients | `TSK-P02-03`, `TSK-P03-03`, `TSK-P04-03` | **11.0 hrs** |
| **Atharv Trivedi** | 202401230 | Fault Injection Subsystem | Case 1 Server & Case 2 Client | `TSK-P04-01`, `TSK-P04-02`, `TSK-P05-03` | **10.0 hrs** |
| **Shlok Topaliya** | 202401229 | Parallel Work Partitioning | Case 2 Enhanced & Plotting | `TSK-P02-04`, `TSK-P06-01`, `TSK-P06-02`, `TSK-P08-01` | **14.5 hrs** |
| **Yug Malaviya** | 202401256 | Test Automation & Edge Cases | Benchmark Runner & Final Report | `TSK-P07-01`, `TSK-P07-02`, `TSK-P08-02` | **13.5 hrs** |

---

## 2. Detailed Member Work Breakdown

---

### 2.1 Jay Vaghela (ID: 202401237)
- **Primary Responsibility**: Protocol Wire Framing Engine & Topic Directory Security
- **Secondary Responsibility**: Part 1 Single-Threaded and Multi-Threaded Server Lifecycle
- **Assigned Tasks**:
  - `TSK-P02-01`: Core Framing & Socket Helper Routines (`send_all`, `recv_all`, `send_frame`, `recv_frame`) [4.0 hrs]
  - `TSK-P02-02`: Topic Discovery, Manifest Builder & Path Traversal Security (`common/topic_manager.*`) [3.0 hrs]
  - `TSK-P03-01`: Part 1 Single-Threaded Server Implementation (`Part1/server.c`) [3.5 hrs]
  - `TSK-P03-02`: Part 1 Multi-Threaded Server Implementation (`Part1/server.c` MT mode) [3.0 hrs]
- **Deliverables**:
  - `common/protocol.h`, `common/protocol.c`
  - `common/topic_manager.h`, `common/topic_manager.c`
  - `Part1/server.c`, `Part1/Makefile`
- **Dependencies**: Depends on repo initialization (`TSK-P01-01`).
- **Reviewer Duties**: Reviews Vivek Dodiya's `file_transfer.*` and `Part1/client.c`.
- **Testing Duties**: Unit tests for framing edge cases (1-byte partial I/O) and path traversal attack tests.
- **Estimated Workload**: 13.5 engineering hours.

---

### 2.2 Dharmraj Vaghela (ID: 202401236) — Repository & Integration Lead
- **Primary Responsibility**: In-Memory Session Management Engine & Checkpoint Synchronization
- **Secondary Responsibility**: Part 2 Case 2 Server Implementation & Git Governance
- **Assigned Tasks**:
  - `TSK-P01-01`: Repository Bootstrap, Branch Protection & CI Setup [2.0 hrs]
  - `TSK-P05-01`: Thread-Safe In-Memory Session & Checkpoint Engine (`common/session.*`) [4.5 hrs]
  - `TSK-P05-02`: Part 2 Case 2 Server Implementation (`Part2_Case2/server.c`) [4.0 hrs]
  - `TSK-P08-03`: Packaging, Clean Build Verification & Archive Generation (`scripts/package_submission.sh`) [2.5 hrs]
- **Deliverables**:
  - Initial repository layout and branch rules
  - `common/session.h`, `common/session.c`
  - `Part2_Case2/server.c`, `Part2_Case2/Makefile`
  - `scripts/package_submission.sh`, `FinalSubmission_Group_071_072.zip`
- **Dependencies**: Depends on `protocol.h`, `topic_manager.h`, `fault_injection.h`.
- **Reviewer Duties**: Reviews Jay Vaghela's `Part1/server.c` and Atharv Trivedi's `Part2_Case2/client.c`.
- **Testing Duties**: Multi-threaded concurrency testing on session table under high contention.
- **Estimated Workload**: 15.0 engineering hours.

---

### 2.3 Vivek Dodiya (ID: 202401253)
- **Primary Responsibility**: Bounded File I/O Engine & Directory Assembly
- **Secondary Responsibility**: Client Architecture (Part 1 Client & Part 2 Case 1 Reconnect Client)
- **Assigned Tasks**:
  - `TSK-P02-03`: Bounded File I/O & Streaming Subsystem (`common/file_transfer.*`) [4.0 hrs]
  - `TSK-P03-03`: Part 1 Client Implementation (`Part1/client.c`) [3.5 hrs]
  - `TSK-P04-03`: Part 2 Case 1 Client with Full Topic Restart Loop (`Part2_Case1/client.c`) [3.5 hrs]
- **Deliverables**:
  - `common/file_transfer.h`, `common/file_transfer.c`
  - `Part1/client.c`, `Part1/Makefile`
  - `Part2_Case1/client.c`, `Part2_Case1/Makefile`
- **Dependencies**: Depends on `protocol.h` and `topic_manager.h`.
- **Reviewer Duties**: Reviews Atharv Trivedi's `fault_injection.*` and Shlok Topaliya's `Case 2 Enhanced Client`.
- **Testing Duties**: Verification of file integrity (`diff -r`) for binary files (JPEG/PNG) and empty files.
- **Estimated Workload**: 11.0 engineering hours.

---

### 2.4 Atharv Trivedi (ID: 202401230)
- **Primary Responsibility**: Fault Injection Subsystem & Case 1 Abrupt Disconnection
- **Secondary Responsibility**: Part 2 Case 2 Client Checkpoint Tracking & Resume Loop
- **Assigned Tasks**:
  - `TSK-P04-01`: Fault Injection Engine (`common/fault_injection.*`) [3.0 hrs]
  - `TSK-P04-02`: Part 2 Case 1 Server Implementation (`Part2_Case1/server.c`) [3.0 hrs]
  - `TSK-P05-03`: Part 2 Case 2 Client Implementation (`Part2_Case2/client.c`) [4.0 hrs]
- **Deliverables**:
  - `common/fault_injection.h`, `common/fault_injection.c`
  - `Part2_Case1/server.c`, `Part2_Case1/Makefile`
  - `Part2_Case2/client.c`, `Part2_Case2/Makefile`
- **Dependencies**: Depends on `protocol.h`, `file_transfer.h`, `session.h`.
- **Reviewer Duties**: Reviews Vivek Dodiya's `Part2_Case1/client.c` and Dharmraj Vaghela's `Part2_Case2/server.c`.
- **Testing Duties**: Statistical testing of fault injection probabilities (`prob=0.0`, `prob=0.01`, `prob=0.10`, `prob=1.0`).
- **Estimated Workload**: 10.0 engineering hours.

---

### 2.5 Shlok Topaliya (ID: 202401229)
- **Primary Responsibility**: Case 2 Enhanced Parallel TCP Coordinator & Work Partitioning
- **Secondary Responsibility**: Thread-Safe Logging & Result Plotting Scripts
- **Assigned Tasks**:
  - `TSK-P02-04`: Thread-Safe Logging & High-Precision Timing (`common/logging.*`) [2.5 hrs]
  - `TSK-P06-01`: Parallel Work Partitioning Module (`common/work_plan.*`) [3.5 hrs]
  - `TSK-P06-02`: Part 2 Case 2 Enhanced Server & Client (`Part2_Case2_Enhanced/*`) [5.5 hrs]
  - `TSK-P08-01`: Graph Plotting Subsystem (`scripts/plot_results.py`) [3.0 hrs]
- **Deliverables**:
  - `common/logging.h`, `common/logging.c`
  - `common/work_plan.h`, `common/work_plan.c`
  - `Part2_Case2_Enhanced/server.c`, `client.c`, `Makefile`
  - `scripts/plot_results.py`
- **Dependencies**: Depends on `session.h`, `file_transfer.h`, `protocol.h`.
- **Reviewer Duties**: Reviews Jay Vaghela's `topic_manager.*` and Yug Malaviya's benchmark scripts.
- **Testing Duties**: Testing parallel connection concurrency, work unit boundary safety, and range overlap prevention.
- **Estimated Workload**: 14.5 engineering hours.

---

### 2.6 Yug Malaviya (ID: 202401256)
- **Primary Responsibility**: Automated Correctness Test Suite & Benchmark Experiment Runner
- **Secondary Responsibility**: Consolidated Final Report Compilation & Contribution Tracking
- **Assigned Tasks**:
  - `TSK-P07-01`: Automated Correctness & Edge Case Test Suite (`scripts/test_all_correctness.sh`) [4.0 hrs]
  - `TSK-P07-02`: Benchmark Experiment Runner & Data Collection (`scripts/run_benchmarks.py`) [4.5 hrs]
  - `TSK-P08-02`: Consolidated Final Report Compilation (`FinalReport_Group_071_072.pdf`) [5.0 hrs]
- **Deliverables**:
  - `scripts/test_all_correctness.sh`
  - `scripts/run_benchmarks.py`, raw CSV logs in `Results/`
  - `FinalReport_Group_071_072.pdf`
- **Dependencies**: Depends on all four server/client executables being operational.
- **Reviewer Duties**: Reviews Shlok Topaliya's `logging.*` and Dharmraj Vaghela's packaging script.
- **Testing Duties**: End-to-end multi-host regression testing and automated test execution across all 4 directories.
- **Estimated Workload**: 13.5 engineering hours.

---

## 3. Peer Review Matrix

To prevent code integration bugs, every Pull Request must receive an approving review from an assigned partner before merging into `develop`:

| Feature Area | Author | Mandatory Reviewer | Secondary Reviewer |
| :--- | :--- | :--- | :--- |
| **Protocol & Sockets** | Jay Vaghela | Vivek Dodiya | Dharmraj Vaghela |
| **Topic Manifest & Paths** | Jay Vaghela | Shlok Topaliya | Yug Malaviya |
| **File I/O & Streaming** | Vivek Dodiya | Jay Vaghela | Shlok Topaliya |
| **Part 1 Server (Seq & MT)** | Jay Vaghela | Atharv Trivedi | Dharmraj Vaghela |
| **Part 1 Client** | Vivek Dodiya | Jay Vaghela | Atharv Trivedi |
| **Fault Injection Engine** | Atharv Trivedi | Vivek Dodiya | Dharmraj Vaghela |
| **Part 2 Case 1 Server** | Atharv Trivedi | Dharmraj Vaghela | Vivek Dodiya |
| **Part 2 Case 1 Client** | Vivek Dodiya | Atharv Trivedi | Jay Vaghela |
| **Session Table Engine** | Dharmraj Vaghela | Jay Vaghela | Shlok Topaliya |
| **Part 2 Case 2 Server** | Dharmraj Vaghela | Atharv Trivedi | Jay Vaghela |
| **Part 2 Case 2 Client** | Atharv Trivedi | Dharmraj Vaghela | Vivek Dodiya |
| **Case 2 Enhanced Parallel** | Shlok Topaliya | Vivek Dodiya | Dharmraj Vaghela |
| **Logging & Metrics** | Shlok Topaliya | Yug Malaviya | Jay Vaghela |
| **Automated Test Suite** | Yug Malaviya | Atharv Trivedi | Vivek Dodiya |
| **Benchmark Runner** | Yug Malaviya | Shlok Topaliya | Dharmraj Vaghela |
| **Final Report & Packaging**| Yug Malaviya / Dharmraj V. | Jay Vaghela | All Members |
