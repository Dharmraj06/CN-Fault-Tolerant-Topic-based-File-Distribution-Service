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

## Project Roadmap & Documentation

The complete implementation plan, architecture, wire protocol specifications, member task allocations, and daily execution schedules are organized under `docs/roadmap/`:

* **[Master Roadmap Entry Point](docs/roadmap/README.md)**: Overview, sitemap, and recommended reading order.
* **[Project Audit & Status](docs/roadmap/PROJECT_AUDIT.md)**: Current state, document hierarchy, requirements classification, and risk mitigations.
* **[Protocol Specification](docs/roadmap/PROTOCOL_SPECIFICATION.md)**: 16-byte fixed binary header, message wire formats, and sequence diagrams.
* **[Architecture & Interfaces](docs/roadmap/ARCHITECTURE_AND_INTERFACES.md)**: Concrete POSIX C header definitions and module contracts.
* **[Phased Master Roadmap](docs/roadmap/MASTER_ROADMAP.md)**: Detailed task catalog (`TSK-P01-01` through `TSK-P08-03`) with DoD.
* **[Team Task Allocation](docs/roadmap/TEAM_TASK_ALLOCATION.md)**: Balanced workload matrix and peer review pairings for all 6 members.
* **[Daily Execution Plan](docs/roadmap/DAILY_EXECUTION_PLAN.md)**: Day-by-day and hour-by-hour calendar schedule (12–16 October 2026).
* **[Testing & Acceptance Plan](docs/roadmap/TESTING_AND_ACCEPTANCE.md)**: Automated test matrices, correctness checks, and edge-case verification.
* **[Performance Experiment Plan](docs/roadmap/PERFORMANCE_EXPERIMENT_PLAN.md)**: Formal metric formulas, CSV schemas, and plotting specifications.
* **[GitHub Workflow](docs/roadmap/GITHUB_WORKFLOW.md)**: Branching policy, PR rules, Conventional Commits, and code review criteria.
* **[Final Submission Checklist](docs/roadmap/FINAL_SUBMISSION_CHECKLIST.md)**: Directory layout, grader compliance, and packaging automation.

---

## Directory Structure Overview

The four required implementation variants are isolated in independent directories:
- `Part1/` — Single-threaded and multi-threaded baseline topic servers and client.
- `Part2_Case1/` — Multi-threaded server with fault injection and complete topic restart recovery.
- `Part2_Case2/` — Multi-threaded server with in-memory session management and chunk checkpoint resumption.
- `Part2_Case2_Enhanced/` — High-performance transfer using parallel TCP connections and work unit partitioning.
- `Results/` — Raw CSV benchmark logs and generated performance graphs.
