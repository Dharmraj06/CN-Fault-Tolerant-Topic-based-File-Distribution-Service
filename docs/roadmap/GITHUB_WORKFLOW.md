# GITHUB COLLABORATION & WORKFLOW SPECIFICATION
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Team**: Group 071 + Group 072 (6 Students)  
**Repository**: [https://github.com/Dharmraj06/CN-Fault-Tolerant-Topic-based-File-Distribution-Service](https://github.com/Dharmraj06/CN-Fault-Tolerant-Topic-based-File-Distribution-Service)  

---

## 1. Branching Strategy

To enable simultaneous engineering across all six students without merge conflicts or code breakage, the team uses a structured **Feature-Branch Workflow**:

```
[main] ───────────────────────────┬───────────────────────────────► (v1.0 Release: 16 Oct)
                                  │
[develop] ───────────┬────────────┴─────────────┬─────────────────►
                     │                          │
[task/p02-protocol] ─┴──────► (PR #1)           │
                                                │
[task/p03-part1-srv] ───────────────────────────┴──────► (PR #2)
```

1. **`main` (Production / Stable Release Branch)**:
   - Contains only fully verified, submission-ready code.
   - Strictly protected. No direct commits allowed.
   - Merged only from `develop` at major project milestones.
2. **`develop` (Integration Branch)**:
   - Primary staging branch for all ongoing feature integration.
   - Protected branch. All contributions enter `develop` via Pull Requests.
3. **Feature Branches (`task/<task_id>-<short-description>`)**:
   - Short-lived working branches branched off the latest `develop`.
   - Examples:
     - `task/p02-protocol-engine`
     - `task/p03-part1-seq-server`
     - `task/p05-session-engine`

---

## 2. Commit Message Conventions

All team commits must follow the Conventional Commits format, explicitly linking the Task ID from `MASTER_ROADMAP.md`:

```
<type>(<scope>): <short description> [<Task-ID>]

[Optional detailed body explaining systems rationale]
```

### 2.1 Types
- `feat`: A new functional capability or module (e.g., `feat(session): add checkpoint commit under mutex [TSK-P05-01]`)
- `fix`: A bug fix or edge-case handling (e.g., `fix(protocol): handle partial read in recv_frame [TSK-P02-01]`)
- `test`: Adding or modifying automated test cases (e.g., `test(harness): add 0-byte file edge case test [TSK-P07-01]`)
- `docs`: Documentation, roadmap, or report updates (e.g., `docs(roadmap): update daily execution plan [TSK-P01-01]`)
- `refactor`: Code reorganization without functional change
- `build`: Makefile or compilation script changes

---

## 3. Pull Request & Code Review Rules

1. **PR Title Format**:
   `[<Task-ID>] <Brief summary of deliverable>`  
   *Example*: `[TSK-P02-01] Implement Core Framing & send_all / recv_all Sockets Engine`
2. **Mandatory Peer Review**:
   - Every PR requires **at least 1 approving review** from the assigned peer reviewer (see `TEAM_TASK_ALLOCATION.md`) before merge.
   - PR author cannot approve or merge their own PR.
3. **Pre-Merge Validation Checklist (Self-Check before requesting review)**:
   - [ ] Code compiles with `make clean && make` with **zero warnings** (`-Wall -Wextra`).
   - [ ] Unit tests pass locally.
   - [ ] No hardcoded ports, IP addresses, or file paths exist.
   - [ ] Code follows POSIX C conventions and frees all allocated memory.
4. **Merge Strategy**:
   - Use **"Squash and merge"** or **"Rebase and merge"** to keep `develop` history linear and clean.
   - Delete the feature branch immediately after merging.

---

## 4. Protected Shared Interfaces Policy

The headers in `common/` (`protocol.h`, `topic_manager.h`, `file_transfer.h`, `session.h`, `fault_injection.h`, `logging.h`, `work_plan.h`) are **Protected Interfaces**.

- Any proposed change to a function signature, struct layout, or constant in `common/` **requires sign-off from all members whose modules depend on it**.
- Silent breaking changes in headers are strictly prohibited.

---

## 5. Conflict Resolution Workflow

If a feature branch falls behind `develop`:
```bash
# 1. Fetch latest changes
git checkout develop
git pull origin develop

# 2. Switch to your feature branch and rebase
git checkout task/my-feature-branch
git rebase develop

# 3. If conflicts arise, resolve them in your editor, then:
git add <resolved-files>
git rebase --continue

# 4. Verify clean compilation and tests
make clean && make
./scripts/test_feature.sh

# 5. Push to remote feature branch
git push origin task/my-feature-branch --force-with-lease
```

---

## 6. Recording Design Deviations

The course final report requires documenting any deviations between the submitted design document (`Design_Document_Group_071_072_.pdf`) and the actual final code.

- Whenever an implementation choice deviates from the original design document (e.g., adjusting a struct field or adopting the `MSG_START` handshake), the author must document it in `docs/roadmap/DESIGN_DEVIATIONS.md` using the following schema:
  - **Feature / Component**:
  - **Submitted Design Proposal**:
  - **Implemented Design**:
  - **Systems Justification / Reason**:
