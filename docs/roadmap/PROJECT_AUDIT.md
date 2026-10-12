# PROJECT AUDIT: FAULT-TOLERANT TOPIC-BASED FILE DISTRIBUTION SERVICE
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Group**: Group 071 + Group 072 (Merged Lab Team)  
**Date of Audit**: 12 October 2026  
**Final Submission Deadline**: 16 October 2026 (23:59 IST)  
**Repository**: [https://github.com/Dharmraj06/CN-Fault-Tolerant-Topic-based-File-Distribution-Service](https://github.com/Dharmraj06/CN-Fault-Tolerant-Topic-based-File-Distribution-Service)

---

## 1. Executive Summary & Repository Status

A full physical audit of the local working directory and the remote GitHub repository was conducted on 12 October 2026.

### 1.1 Git Working Tree & Remote Status
- **Local Git Repository**: Initialized at `c:\Users\vaghe\OneDrive\Desktop\DA-IICT\SEM-V\CN\Project\CN-Fault-Tolerant-Topic-based-File-Distribution-Service`.
- **Current Branch**: `main`.
- **Commit History**: Exactly **0 commits** (empty repository).
- **Remote Configuration**: `origin -> https://github.com/Dharmraj06/CN-Fault-Tolerant-Topic-based-File-Distribution-Service.git`.
- **Remote References**: `git ls-remote` returns empty. The remote repository has been created by Dharmraj Vaghela (`Dharmraj06`) but no branches or files have been pushed yet.
- **Audit Classification**: *Clean Slate / Initial Setup Phase*.

### 1.2 Workspace Artifacts Inspected
The parent workspace directory contains the following primary specification and design documents:
1. `IT305-lab-socket-programming-project-assignment (1).pdf` (116 KB) — The authoritative course assignment specification defining all mandatory functional requirements, evaluation criteria, and submission rules.
2. `Design_Document_Group_071_072_.pdf` (621 KB, 46 pages) — The comprehensive design document submitted by Group 071 + 072 on 27 September 2026.
3. `key-IT305-lab-socket-programming-project-assignment.pdf` (147 KB, 8 pages) — The instructor reference document titled *"Sample Design of Socket Programming Project"*, issued as post-submission guidance.

---

## 2. Document Hierarchy & Precedence Rules

To prevent conflicting implementations across team members, the following priority order is established:

```
┌────────────────────────────────────────────────────────┐
│  1. Original Assignment Specification (PDF)            │  <-- MANDATORY GROUND TRUTH
│     - CLI parameters, folder names, zip naming, etc.   │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│  2. Group Submitted Design Document (PDF, 46 pages)    │  <-- ARCHITECTURAL BASELINE
│     - 16-byte binary framing, thread models, sessions   │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│  3. Instructor Sample Design Document (PDF, 8 pages)   │  <-- GUIDANCE & INSTRUCTOR INTENT
│     - Simplistic model; validates advanced extensions  │
└────────────────────────────────────────────────────────┘
```

1. **Original Assignment (`IT305-lab...pdf`) is the Absolute Authority**:
   - CLI flags, executable names (`server`, `client`), folder names (`Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`, `Results/`), and output directory path formats (`<output_dir>/<topic>/`) cannot be changed under any circumstances. Automated grading scripts depend on exact compliance.
2. **Team Design Document (`Design_Document_Group...pdf`) is the Implementation Baseline**:
   - Defines the internal architecture, thread synchronization, session table semantics, chunked transfer invariants, and parallel TCP connection strategy.
3. **Instructor Sample Design (`key-IT305...pdf`) is Guidance, Not a Replacement**:
   - The instructor explicitly noted on page 7: *"Note: We have seen few of your submissions, and some designs are really good – even better than this simplistic minimal model design."* This confirms that our team's submitted design is accepted and preferred over the minimal sample.

---

## 3. Discrepancy Analysis & Protocol Reconciliation

A critical comparative analysis between the Team Design Document and the Instructor Sample Design reveals key differences that must be reconciled before writing code.

### 3.1 Comparison Matrix

| Protocol Dimension | Group Submitted Design | Instructor Sample Design | Reconciled Technical Decision | Rationale |
| :--- | :--- | :--- | :--- | :--- |
| **Wire Framing** | 16-byte fixed binary header (`magic`, `version`, `type`, `flags`, `reserved`, `payload_len`) followed by exact binary payload. | Generic `Header Payload` concept; minimum fields: `type`, `transaction_id`, `payload_len`. | **Adopt Group 16-Byte Header with Transaction/Seq ID**. | TCP is an unformatted byte stream. Length framing is strictly necessary to prevent message boundary distortion with binary image files (`animals10`). |
| **Initial Handshake** | Client sends `GET(topic)` -> Server replies `TOPIC_OK(manifest)` or `TOPIC_ERROR`. Transfer starts immediately. | Client sends `GET(topic)` -> Server sends `OK(manifest)` -> Client sends `START(topic)` -> Server streams files. | **Harmonize Handshake**: Client sends `GET(topic)`, Server sends `OK(manifest)`, Client sends `START(topic)`, Server streams. | Incorporating `START` allows the client to prepare the local output directory before the server floods socket buffers, matching the instructor's sample workflow while preserving our binary framing. |
| **File Demarcation** | Server sends `FILE_BEGIN`, then bounded `FILE_DATA` chunks, then `FILE_END`. | Server sends `FILE_HDR`, then `DATA` chunks, acknowledges with `ACK`, ends topic with `CLOSE`. | **Use `FILE_HDR` (or `FILE_BEGIN`), `DATA`, `FILE_END`, `CLOSE` / `TRANSFER_END`**. | Explicit file begin/end control frames allow clean handling of 0-byte files, seek points, and per-file completion barriers. |
| **Session Identification** | Explicit 64-bit pseudo-random `session_id`, sent in `SESSION_START` / `SESSION_RESUME`. | 32-bit or 64-bit `session_id` returned in `OK` message; reused in all subsequent messages. | **64-bit `session_id` passed in `OK` frame and embedded in header/payload**. | Satisfies both designs. Allows `GET` frame to optionally include `session_id` (0 for new transfer, non-zero for resume). |
| **Checkpoint Commit Point** | Explicit `CHUNK_ACK` sent by client after disk write; server commits under mutex. | Periodic `ACK` from client to server (after file or chunk); server updates session state. | **Server commits checkpoint upon receiving `CHUNK_ACK` after every chunk**. | Guarantees bounded retransmission. Client never advances server authority arbitrarily. |
| **Enhancement Choice** | Parallel TCP connections dividing files and byte ranges; conservative 1-connection default. | Non-blocking sockets OR Parallel TCP connections. | **Parallel TCP Connections** (as documented in our submitted design). | Directly satisfies course requirements, avoids massive async state-machine rewrites, and enables true multi-socket transfer scaling. |

---

## 4. Current Component Status Classification

Every component and requirement is classified under one of the five mandatory states:
1. **Implemented and verified**
2. **Implemented but not verified**
3. **Partially implemented**
4. **Planned but not implemented**
5. **Unresolved design decision**

| Requirement / Module | Description | Status | Evidence / Notes |
| :--- | :--- | :--- | :--- |
| **CLI Argument Parsing** | Strict CLI compliance for Server & Client across all parts | **Planned but not implemented** | Specified in Section 19 of Design Doc; no code written yet. |
| **POSIX Framing Helpers** | `send_all()`, `recv_all()`, `send_frame()`, `recv_frame()` | **Planned but not implemented** | Core socket utility required across all 4 directories. |
| **Topic & Manifest Manager** | Scan directory, sort lexicographically, build manifest, path traversal guard | **Planned but not implemented** | Needs `opendir()`, `readdir()`, `stat()`, `qsort()`. |
| **File I/O Engine** | Chunked reading, seeking, atomic writes, `pwrite()` | **Planned but not implemented** | Critical for binary file integrity and resume. |
| **Part 1 Sequential Server** | Single-threaded iterative server (`accept` -> serve -> `close`) | **Planned but not implemented** | Baseline implementation for benchmarking. |
| **Part 1 Multi-Threaded Server**| Concurrent server (`accept` -> `pthread_create` detached) | **Planned but not implemented** | Baseline multi-client concurrency. |
| **Part 1 Client** | Connect, send `GET`, create `<output_dir>/<topic>/`, write files | **Planned but not implemented** | Standard client implementation. |
| **Fault Injection Subsystem** | Probabilistic connection drop (`[0.0, 1.0]`) before chunk send | **Planned but not implemented** | Requires pseudo-random generator, clean socket closure. |
| **Part 2 Case 1 Server & Client**| Abrupt drop + Full topic restart from file 0, offset 0 | **Planned but not implemented** | Client must clean partially written files on reconnect. |
| **In-Memory Session Table** | Thread-safe session table, 64-bit IDs, TTL expiration, mutex locks | **Planned but not implemented** | Server-authoritative checkpoint tracking. |
| **Part 2 Case 2 Server & Client**| Chunk-level checkpointing, `CHUNK_ACK`, resume from byte offset | **Planned but not implemented** | Resume protocol handles client/server state alignment. |
| **Case 2 Enhanced Coordinator**| Parallel TCP connection pooling, work-unit partitioning, reassembly | **Planned but not implemented** | Parallel file transfer and split-file chunk reassembly. |
| **Logging & CSV Metrics** | Monotonic timers (`CLOCK_MONOTONIC`), byte counters, CSV logging | **Planned but not implemented** | Required for raw performance data collection. |
| **Automated Test Suite** | Bash/Python test scripts for correctness, edge cases, and fault recovery | **Planned but not implemented** | Scripts needed for loopback and multi-node test runs. |
| **Performance Benchmark Suite**| Scripts to execute test matrix, collect CSVs, plot throughput/overhead | **Planned but not implemented** | Required to generate final report graphs. |
| **Final Consolidated Report** | LaTeX/Markdown report with tables, graphs, contributions, and deviations | **Planned but not implemented** | Final deliverable `FinalReport_Group_071_072.pdf`. |

---

## 5. Known Risks & Technical Pitfalls

1. **Partial TCP Send/Recv**:
   - *Risk*: `send()` and `recv()` can transfer fewer bytes than requested. Assuming a single `recv()` returns a full frame will cause immediate desynchronization.
   - *Mitigation*: All network I/O must strictly go through `send_all()` and `recv_all()` loops.
2. **Directory Traversal Vulnerability**:
   - *Risk*: A malicious or malformed client sends `GET ../../etc/passwd` or filenames containing `/`.
   - *Mitigation*: Topic names and filenames must be strictly validated to contain no `/`, `\`, `..`, or control characters.
3. **Deadlocks in Multi-Threaded Session State**:
   - *Risk*: Worker threads holding the global session mutex during slow disk or socket I/O will freeze the entire server.
   - *Mitigation*: Fine-grained locking: acquire session mutex only to look up, clone, or commit checkpoint state; release lock before any disk or network operation.
4. **Data Corruption During Parallel TCP File Reassembly (Enhanced)**:
   - *Risk*: Two connections writing to overlapping byte ranges of the same file.
   - *Mitigation*: Strict work-unit partitioning: non-overlapping chunk ranges, distinct destination file handles, or thread-safe `pwrite()` at pre-assigned offsets.
5. **SIGPIPE Crashing Server/Client**:
   - *Risk*: Writing to a socket after an injected or unexpected disconnection triggers `SIGPIPE` and kills the process.
   - *Mitigation*: Server and client must set `signal(SIGPIPE, SIG_IGN)` or use `MSG_NOSIGNAL` on all `send()` calls.
6. **Strict 4-Day Timeline (12–16 Oct)**:
   - *Risk*: Integration bottlenecks occurring late on 15 or 16 October.
   - *Mitigation*: Strict modular encapsulation. Freeze `protocol.h` and helper headers on Day 1 so all 6 members work simultaneously without cross-blocking.
