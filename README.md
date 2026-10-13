# Fault-Tolerant Topic-Based File Distribution Service

**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Academic Institution**: Dhirubhai Ambani Institute of Information and Communication Technology (DA-IICT)  
**Group**: Merged Group 071 + Group 072  

## Team Members

1. **Jay Vaghela** — 202401237
2. **Dharmraj Vaghela** — 202401236 (Repository Owner)
3. **Vivek Dodiya** — 202401253
4. **Atharv Trivedi** — 202401230
5. **Shlok Topaliya** — 202401229
6. **Yug Malaviya** — 202401256

---

## Project Documentation & Architecture Guides

The project governance, execution roadmap, codebase architecture, and development guidelines are organized as follows:
* **[Master Phase-Wise Execution Roadmap](PART2_TASKS_ROADMAP.md)**: Authoritative single source of truth for phase tasks, frozen protocol contracts, acceptance gates, requirement traceability, and packaging rules.
* **[Codebase Structure & Architecture Reference](docs/architecture/CODEBASE_STRUCTURE.md)**: Comprehensive guide to directory responsibilities, file-by-file matrix, module dependencies, runtime data flows, and testing map.
* **[Development Workflow & Handoff Protocol](docs/development/DEVELOPMENT_WORKFLOW.md)**: Sequential execution governance, phase entry checklists, Git branching conventions, testing routines, and handoff templates.
* **[College-Level C Coding Guidelines & Viva Guide](docs/development/COLLEGE_LEVEL_CODING_GUIDELINES.md)**: Standards for undergraduate systems C programming, socket conventions, thread safety, error handling, and peer review.

---

## Sequential Engineering Ownership Model

Execution proceeds through a strictly linear, single-owner phase dependency chain:
**Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5 → Phase 6**

| Phase | Responsible Member | Student ID | Core Engineering Deliverable | Primary Files Owned |
|:---:|---|:---:|---|---|
| **Phase 1** | **Jay Vaghela** | 202401237 | Wire Protocol Framing Engine & Part 1 Baseline Server | `common/protocol.*`, `common/topic_manager.*`, `Part1/server.c` |
| **Phase 2** | **Vivek Dodiya** | 202401253 | Bounded File Streaming Engine & Part 1 Baseline Client | `common/file_transfer.*`, `Part1/client.c` |
| **Phase 3** | **Atharv Trivedi** | 202401230 | Fault Injection Subsystem & Part 2 Case 1 End-to-End | `common/fault_injection.*`, `Part2_Case1/server.c`, `Part2_Case1/client.c` |
| **Phase 4** | **Dharmraj Vaghela** | 202401236 | In-Memory Session Management & Part 2 Case 2 End-to-End | `common/session.*`, `Part2_Case2/server.c`, `Part2_Case2/client.c` |
| **Phase 5** | **Shlok Topaliya** | 202401229 | Parallel TCP Architecture & Part 2 Case 2 Enhanced End-to-End | `common/work_plan.*`, `Part2_Case2_Enhanced/server.c`, `Part2_Case2_Enhanced/client.c` |
| **Phase 6** | **Yug Malaviya** | 202401256 | Logging, Regression Harness, Benchmarks, Report & Packaging | `common/logging.*`, `scripts/*`, `Results/*`, `FinalReport_Group_071_072.pdf` |

---

## Directory Structure Overview

The four required implementation variants are isolated in independent directories, each featuring its own `Makefile` producing runnable `server` and `client` binaries:
- `Part1/` — Single-threaded and multi-threaded baseline topic servers and client.
- `Part2_Case1/` — Multi-threaded server with fault injection and complete topic restart recovery.
- `Part2_Case2/` — Multi-threaded server with in-memory session management and chunk checkpoint resumption.
- `Part2_Case2_Enhanced/` — High-performance transfer using parallel TCP connections and work unit partitioning.
- `Results/` — Raw CSV benchmark logs and generated performance graphs.
- `scripts/` — Automated test harness, benchmark runners, plotting scripts, and packaging automation.

---

## Standard CLI Invocations (Mandatory Grader Compliance)

### Server Programs
- **Part 1:** `./server <port> <topics_root_dir>`
- **Part 2 (All Cases):** `./server <port> <topics_root_dir> <failure probability>`

### Client Programs
- **All Cases:** `./client <server_ip> <server_port> <topic> <output_dir> [num_connections]`
  *(Note: For Case 2 Enhanced, the optional `[num_connections]` argument defaults strictly to 1 if omitted, ensuring 100% compatibility with automated grading scripts).*
