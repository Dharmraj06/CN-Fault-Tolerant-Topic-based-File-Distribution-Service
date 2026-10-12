# FAULT-TOLERANT TOPIC-BASED FILE DISTRIBUTION SERVICE
## MASTER PROJECT ROADMAP & TEAM EXECUTION SUITE

**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Academic Institution**: Dhirubhai Ambani Institute of Information and Communication Technology (DA-IICT)  
**Team**: Merged Group 071 + Group 072 (6 Students)  
**Final Submission Deadline**: 16 October 2026 (23:59 IST)  
**Repository**: [https://github.com/Dharmraj06/CN-Fault-Tolerant-Topic-based-File-Distribution-Service](https://github.com/Dharmraj06/CN-Fault-Tolerant-Topic-based-File-Distribution-Service)  

---

## 1. Welcome & Purpose of this Documentation

This directory contains the complete, battle-tested, implementation-oriented execution roadmap for our team's course project. Every specification, interface contract, test matrix, and daily assignment here is grounded directly in:
1. The **Authoritative Course Assignment Specification** (`IT305-lab-socket-programming-project-assignment (1).pdf`)
2. Our **Submitted Team Design Document** (`Design_Document_Group_071_072_.pdf`, 46 pages)
3. The **Instructor Reference Design** (`key-IT305-lab-socket-programming-project-assignment.pdf`)

With only 4 calendar days remaining until the 16 October deadline, this suite provides unambiguous technical guidance so that each of the six team members can implement, test, and review code independently without integration bottlenecks.

---

## 2. Document Sitemap & Recommended Reading Order

Team members should read and consult the documents in the following order:

```
                                  START HERE
                                      │
                         ┌────────────▼────────────┐
                         │ docs/roadmap/README.md  │
                         └────────────┬────────────┘
                                      │
            ┌─────────────────────────┼─────────────────────────┐
            │                         │                         │
┌───────────▼────────────┐┌───────────▼────────────┐┌───────────▼────────────┐
│ 1. PROJECT_AUDIT.md    ││ 2. ARCHITECTURE_AND_   ││ 3. PROTOCOL_           │
│ Current state, rules,  ││    INTERFACES.md       ││    SPECIFICATION.md    │
│ & reconciled decisions ││ C headers & contracts  ││ 16-byte binary framing │
└───────────┬────────────┘└───────────┬────────────┘└───────────┬────────────┘
            │                         │                         │
            └─────────────────────────┼─────────────────────────┘
                                      │
            ┌─────────────────────────┴─────────────────────────┐
            │                                                   │
┌───────────▼────────────┐                             ┌────────▼────────────┐
│ 4. TEAM_TASK_          │                             │ 5. DAILY_           │
│    ALLOCATION.md       │◄───────────────────────────►│    EXECUTION_PLAN.md│
│ Member tasks & reviews │                             │ Hourly calendar     │
└───────────┬────────────┘                             └────────┬────────────┘
            │                                                   │
            └─────────────────────────┬─────────────────────────┘
                                      │
                         ┌────────────▼────────────┐
                         │ 6. MASTER_ROADMAP.md    │
                         │ Detailed task catalog   │
                         └────────────┬────────────┘
                                      │
            ┌─────────────────────────┼─────────────────────────┐
            │                         │                         │
┌───────────▼────────────┐┌───────────▼────────────┐┌───────────▼────────────┐
│ 7. GITHUB_WORKFLOW.md  ││ 8. TESTING_AND_        ││ 9. PERFORMANCE_        │
│ Branch, PR & review    ││    ACCEPTANCE.md       ││    EXPERIMENT_PLAN.md  │
│ conventions            ││ Test matrices & edge-cs││ Benchmarks & graphs    │
└───────────┬────────────┘└───────────┬────────────┘└───────────┬────────────┘
            │                         │                         │
            └─────────────────────────┼─────────────────────────┘
                                      │
                         ┌────────────▼────────────┐
                         │ 10. FINAL_SUBMISSION_   │
                         │     CHECKLIST.md        │
                         │ Archive packaging & QA  │
                         └─────────────────────────┘
```

### Quick Links to Roadmap Documents
1. [PROJECT_AUDIT.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/PROJECT_AUDIT.md): Comprehensive inventory of repository assets, assignment rules, status classification, and discrepancy resolution.
2. [ARCHITECTURE_AND_INTERFACES.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/ARCHITECTURE_AND_INTERFACES.md): Concrete POSIX C header definitions (`protocol.h`, `topic_manager.h`, `file_transfer.h`, `fault_injection.h`, `session.h`, `logging.h`, `work_plan.h`), thread safety, and memory rules.
3. [PROTOCOL_SPECIFICATION.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/PROTOCOL_SPECIFICATION.md): Wire framing specification, 16-byte fixed binary header, message types, and Mermaid sequence diagrams for all four variants.
4. [TEAM_TASK_ALLOCATION.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/TEAM_TASK_ALLOCATION.md): Six-student engineering distribution, ownership areas, peer review pairings, and workload matrix.
5. [DAILY_EXECUTION_PLAN.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/DAILY_EXECUTION_PLAN.md): Hour-by-hour and day-by-day execution targets covering 12–16 October 2026.
6. [MASTER_ROADMAP.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/MASTER_ROADMAP.md): Detailed task catalog specifying Task IDs (`TSK-P01-01` to `TSK-P08-03`), steps, acceptance criteria, test commands, and definition of done.
7. [GITHUB_WORKFLOW.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/GITHUB_WORKFLOW.md): Git branching model, commit conventions (`feat:`, `fix:`), PR review rules, and conflict resolution policies.
8. [TESTING_AND_ACCEPTANCE.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/TESTING_AND_ACCEPTANCE.md): Full test matrix covering build verification, data integrity (`diff -r`), concurrency, fault injection, edge cases, and automated test script specifications.
9. [PERFORMANCE_EXPERIMENT_PLAN.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/PERFORMANCE_EXPERIMENT_PLAN.md): Mathematical metric definitions, monotonic timing, raw CSV logging schema, and graph generation specs.
10. [FINAL_SUBMISSION_CHECKLIST.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/FINAL_SUBMISSION_CHECKLIST.md): Strict grader compliance checks, folder hierarchy, report requirements, and packaging script (`scripts/package_submission.sh`).
11. [CODING_STANDARDS.md](file:///c:/Users/vaghe/OneDrive/Desktop/DA-IICT/SEM-V/CN/Project/CN-Fault-Tolerant-Topic-based-File-Distribution-Service/docs/roadmap/CODING_STANDARDS.md): C programming mentor guide, POSIX standards, socket patterns, mutex discipline, and viva defense checklist.

---

## 3. Team Member Quick Reference

| Member Name | Student ID | Primary Engineering Ownership | Primary Partner / Reviewer |
| :--- | :--- | :--- | :--- |
| **Jay Vaghela** | 202401237 | Framing Engine & Part 1 Server | Vivek Dodiya |
| **Dharmraj Vaghela** | 202401236 | Session Table & Case 2 Server | Atharv Trivedi |
| **Vivek Dodiya** | 202401253 | File I/O Engine & Part 1/Case 1 Clients | Jay Vaghela |
| **Atharv Trivedi** | 202401230 | Fault Injector & Case 1 Server | Dharmraj Vaghela |
| **Shlok Topaliya** | 202401229 | Parallel Work Coordinator & Case 2 Enhanced | Yug Malaviya |
| **Yug Malaviya** | 202401256 | Test Automation, Benchmarks & Final Report | Shlok Topaliya |

---

## 4. Immediate First Actions for Day 1 (13 October 2026)

1. **Dharmraj Vaghela**: Commit and push this `docs/` tree to GitHub `main`, create `develop` branch, and set up team write access.
2. **Jay Vaghela**: Branch `task/p02-protocol-engine` from `develop`; implement `common/protocol.h` and `common/protocol.c`.
3. **Vivek Dodiya**: Branch `task/p02-file-transfer` from `develop`; implement `common/file_transfer.h` and `common/file_transfer.c`.
4. **Shlok Topaliya**: Branch `task/p02-logging-timing` from `develop`; implement `common/logging.h` and `common/logging.c`.
5. **Atharv Trivedi**: Branch `task/p04-fault-injector` from `develop`; scaffold `common/fault_injection.h` and `common/fault_injection.c`.
6. **Yug Malaviya**: Branch `task/p07-correctness-suite` from `develop`; build dummy dataset generation script `scripts/generate_test_dataset.sh`.
