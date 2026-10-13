# IT305 Computer Networks: Team Development & Execution Workflow

**Project:** Fault-Tolerant Topic-Based File Distribution Service  
**Course:** IT305 Computer Networks Lab (Autumn 2026), DA-IICT  
**Team:** Merged Group 071 + Group 072  
**Document Status:** Operational Workflow Guide & Collaboration Protocol  
**Authority:** Subordinate to [PART2_TASKS_ROADMAP.md](../../PART2_TASKS_ROADMAP.md)  

---

## 1. Core Development Principles

To ensure zero integration friction, rock-solid stability, and guaranteed grader compliance across six team members, all development activities must follow six foundational principles:

1. **Small, Atomic, Understandable Changes:** Focus every commit on a single function, header definition, or test scenario. Never mix architectural refactoring with bug fixes.
2. **Strict Single-Owner Phase Responsibility:** Exactly one member owns each phase. No two members implement code in parallel.
3. **Reproducible Builds and Zero-Warning Policy:** Code must build cleanly using standard Makefiles under `gcc -Wall -Wextra -Werror -pedantic -pthread`. Suppressing compiler warnings via pragma or compiler flags is strictly prohibited.
4. **Frozen Interface Discipline:** Once a header contract is frozen, downstream members consume it without modification. Any required interface modification requires a formal RFC.
5. **Evidence-Based Acceptance Testing:** A task is not complete merely because it compiles. Objective evidence (test output logs, bit-exact `diff -r` results, Valgrind memory leak checks) must be produced before claiming test success.
6. **Honest and Transparent Reporting:** Never hide bugs, failed edge cases, or incomplete features. Unresolved questions and limitations must be documented openly in the phase handoff package.

---

## 2. Strict Sequential Phase Execution Model

Development proceeds through a strictly linear, single-owner engineering chain:

$$\mathbf{Phase\ 1} \longrightarrow \mathbf{Phase\ 2} \longrightarrow \mathbf{Phase\ 3} \longrightarrow \mathbf{Phase\ 4} \longrightarrow \mathbf{Phase\ 5} \longrightarrow \mathbf{Phase\ 6}$$

### 2.1 Transition Governance Rules
No team member may begin implementation of Phase $N+1$ until Member $N$ has satisfied all of the following requirements:
1. **Task Completion:** All tasks assigned to Phase $N$ in [PART2_TASKS_ROADMAP.md](../../PART2_TASKS_ROADMAP.md) are fully implemented.
2. **Mandatory Tests Passed:** All phase-specific unit, integration, and loopback tests pass with exit status 0.
3. **Regression Integrity:** All preceding phases build and continue to pass their respective verification tests.
4. **Handoff Documentation:** The standardized Phase Handoff Package (§8) is authored, detailing build commands, test logs, and known limitations.
5. **Formal Exit Gate Approval:** The phase passes its exit gate review, and Member $N+1$ formally accepts the handoff.

> [!WARNING]
> **Zero Parallel Implementation:** A later phase may depend on concepts from several earlier phases, but **no implementation work may begin early**. Technical dependencies represent prerequisite inputs, not permission to write code concurrently.

---

## 3. Phase Entry Checklist

Before Member $N+1$ begins implementation, they must review and check off every item on the following **Phase Entry Checklist**:

- [ ] **Preceding Handoff Accepted:** Formally received and read Handoff Package $N$ from the previous member.
- [ ] **Repository Cleanliness:** `git status` on the working branch shows zero unstaged changes or untracked scratch files.
- [ ] **Clean Preceding Build:** Running `make clean && make` in all completed implementation folders (`Part1/`, `Part2_Case1/`, etc.) produces zero compiler errors or warnings.
- [ ] **Regression Verification:** Running preceding phase test scripts (e.g., `tests/test_phase1.c`, `tests/test_phase2.sh`) executes cleanly and passes 100%.
- [ ] **Frozen Headers Inspected:** Verified that all shared headers in `common/` consumed by the incoming phase are documented and frozen.
- [ ] **Scope Boundary Confirmed:** Verified the exact list of files to be created or edited; confirmed that no earlier frozen files will be edited without a formal RFC.
- [ ] **Feature Branch Initialized:** Created a dedicated feature branch from the updated integration base.

---

## 4. Git Workflow & Branching Conventions

*(Proposed for Team Approval)*

To guarantee an auditable commit history, eliminate merge conflicts, and protect the buildable state of the repository, the team adheres to the following Git workflow:

### 4.1 Branch Topology
- `main`: Production-ready branch. Contains only reviewed, fully verified phases that pass all exit gates. Must be compilable at all times.
- `develop`: Primary integration branch. Receives completed phase PRs.
- `feature/phase<N>-<shortname>`: Dedicated development branch for each phase owner (e.g., `feature/phase1-framing`, `feature/phase2-client`, `feature/phase3-fault-injection`, `feature/phase4-session-resume`, `feature/phase5-parallel-tcp`, `feature/phase6-benchmarks`).

### 4.2 Branch Creation & Daily Sync Commands
```bash
# 1. Update develop branch
git checkout develop
git pull origin develop

# 2. Create phase feature branch (e.g., Phase 2)
git checkout -b feature/phase2-client

# 3. Verify clean status
git status
```

### 4.3 Commit Standards & Conventional Commits
All commits must be atomic and follow the Conventional Commits specification:
```
<type>(<scope>): <short imperative description>

[optional body explaining technical rationale and invariants]

[optional issue/task reference, e.g., TSK-P02-01]
```

**Permitted Types:**
- `feat`: New feature or implementation module (e.g., `feat(protocol): implement send_all framing loop`).
- `fix`: Bug fix in existing module (e.g., `fix(client): handle partial TCP read on file boundary`).
- `test`: Adding or updating test harnesses (e.g., `test(phase2): add bit-exact diff test script`).
- `docs`: Documentation updates (e.g., `docs(roadmap): update Phase 2 handoff package`).
- `refactor`: Internal code clean-up without changing behavior (e.g., `refactor(session): optimize mutex lock scope`).

### 4.4 Review & Merge Process
1. Upon completing all phase tasks and tests, the phase owner pushes their branch and opens a Pull Request targeting `develop`.
2. The PR description must contain the completed **Phase Handoff Package**.
3. The designated peer reviewer (specified in [PART2_TASKS_ROADMAP.md](../../PART2_TASKS_ROADMAP.md)) conducts a line-by-line review using the [Code Review Checklist](COLLEGE_LEVEL_CODING_GUIDELINES.md#19-code-review-checklist).
4. Upon approval, the PR is merged into `develop` using a non-fast-forward merge commit (`git merge --no-ff`).
5. After Phase 6 verification, `develop` is merged into `main` for final release.

---

## 5. File Ownership & Change Control

### 5.1 Strict Ownership Boundaries
Each file has exactly one designated phase owner (see [CODEBASE_STRUCTURE.md](../architecture/CODEBASE_STRUCTURE.md) §4). 
- Only the phase owner has write authority over that file during their phase.
- Later members may link against shared headers and `.o` objects, but **must not edit earlier source files**.

### 5.2 Formal RFC Process for Shared Headers
If an edge case in Phase $N+1$ necessitates a change to a frozen header created in Phase $M$ ($M < N+1$):
1. **RFC Submission:** The requesting member documents the required change in an RFC issue:
   - Specific lines to modify.
   - Why the change cannot be accommodated in the local module.
   - Impact on preceding implementations.
2. **Review & Voting:** All 6 team members must vote to approve the RFC.
3. **Execution by Original Owner:** The original owner of the file (Member $M$) makes the approved change on a hotfix branch.
4. **Regression Run:** Member $M$ re-runs all tests for Phase $M$ through $N$ to prove zero regressions before the change is merged.

---

## 6. Build and Testing Procedure

Every team member must follow this standardized 9-step build and testing routine before pushing code:

```bash
# Step 1: Inspect working tree
git status -s

# Step 2: Build the target from clean state
make -C <target_directory> clean
make -C <target_directory>

# Step 3: Review compiler warnings (must be zero)
# In case of any warning, fix immediately. Flags: -Wall -Wextra -Werror -pedantic

# Step 4: Run unit tests
./tests/test_<phase>

# Step 5: Run integration tests against server/client
bash tests/test_<phase>.sh

# Step 6: Run failure recovery tests (Part 2 phases)
# Run under fault injection (prob=0.05, prob=0.10) to verify graceful recovery

# Step 7: Run regression check on earlier variants
make -C Part1 && make -C Part2_Case1

# Step 8: Verify memory safety with Valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./<target_binary> <args>

# Step 9: Record logs and results into phase test log
```

---

## 7. Testing & Evidence Retention Policy

A claim of test success is invalid without reproducible evidence. The following artifacts must be preserved in `tests/` or `Results/` as proof of completion:

| Evidence Type | Scope / Source | Retention Location | Requirement for Phase Exit |
|---|---|---|:---:|
| **Build Log** | Output of `make clean && make` | `tests/build_phase<N>.log` | Zero errors, zero warnings. |
| **Unit Test Log** | Framing, security, or session unit tests | `tests/unit_phase<N>.log` | 100% assertions passed. |
| **Integrity Diff Log** | Output of `diff -r <dataset> <download>` | `tests/diff_phase<N>.log` | Zero differences found. |
| **Valgrind Log** | Memory leak and file descriptor tracking | `tests/valgrind_phase<N>.log` | 0 bytes lost, 0 open FD leaks. |
| **Fault Recovery Log** | Server drop and client reconnect trace | `tests/fault_phase<N>.log` | Disconnect recorded; eventual bit-exact completion. |
| **Benchmark CSV** | Monotonic timing and byte measurements | `Results/exp<N>_*.csv` | Authentic measurements present. |

---

## 8. Standard Phase Handoff Procedure & Template

When a member finishes their phase, they must complete the following handoff document and attach it to their Pull Request:

```markdown
# Phase <N> Handoff Package

**Phase Number & Title:** Phase <N>: <Title>
**Phase Owner:** <Full Name> (<Student ID>)
**Receiving Member:** <Full Name> (<Student ID>)
**Handoff Date:** <Date, e.g., 14 October 2026>

### 1. Source Artifacts Delivered
- Files Created:
  - `<path/to/file1.c>`
  - `<path/to/file1.h>`
- Files Modified:
  - `<path/to/Makefile>`

### 2. Compilation & Verification Commands
- Clean Build:
  `make -C <folder> clean && make -C <folder>`
- Test Command Executed:
  `bash tests/test_phase<N>.sh`
- Execution Log Location:
  `tests/phase<N>_verification.log`

### 3. Public Interfaces & Contracts Frozen
- Headers Frozen: `<common/module.h>` (Version: 1.0)
- Key Functions Exposed:
  - `int module_function(args...)`
- Invariants & Preconditions:
  - [e.g., Thread-safe, requires non-null buffer]

### 4. Objective Acceptance Evidence
- [x] Zero compiler warnings under `gcc -Wall -Wextra -Werror -pedantic`.
- [x] Bit-exact `diff -r` match on topic download.
- [x] Zero memory leaks verified under Valgrind.
- [x] Reconnection / recovery verified under fault injection.

### 5. Known Limitations & Edge Cases Handled
- Handled: [e.g., Directory traversal rejected, truncated frames recovered]
- Limitations: [e.g., Max 256 concurrent sessions supported]

### 6. Specific Instructions for Incoming Member
- [e.g., Vivek: Include `common/file_transfer.h` in `Part1/client.c`. Server transmits `MSG_TOPIC_OK` followed by sequential `MSG_FILE_BEGIN` frames.]

### 7. Sign-off
- **Exiting Owner Signature:** <Name> (Date)
- **Receiving Owner Acceptance:** <Name> (Date)
```

---

## 9. Issue & Decision Management

To prevent blocking issues from getting lost in informal chats:
1. **GitHub Issues for Blockers:** Any technical ambiguity in the specification or unexpected system behavior must be recorded as a GitHub Issue labeled `blocker` or `clarification`.
2. **Architecture Decision Records (ADRs):** When an architectural choice is made to resolve an unspecified edge case, it must be documented in [PART2_TASKS_ROADMAP.md](../../PART2_TASKS_ROADMAP.md) §8.1.
3. **Escalation Policy:** If an issue blocks progress for $> 2$ hours, the phase owner calls an emergency standup with Dharmraj (Repository Lead) to resolve the issue or reassign pair-programming support.

---

## 10. Documentation Maintenance Matrix

Documentation must be kept synchronized with code at all times. When a change occurs, update the designated document:

| Project Event | Primary Document to Update | Secondary Documents to Check |
|---|---|---|
| New file or directory added | `docs/architecture/CODEBASE_STRUCTURE.md` | `README.md`, `PART2_TASKS_ROADMAP.md` |
| Shared function signature altered | `common/<header>.h` & `PART2_TASKS_ROADMAP.md` §4 | `CODEBASE_STRUCTURE.md` §4 |
| Phase completed & passed exit gate | `PART2_TASKS_ROADMAP.md` (Update Status) | PR Handoff Note |
| Test command or test script added | `docs/architecture/CODEBASE_STRUCTURE.md` §9 | `PART2_TASKS_ROADMAP.md` §9 |
| Benchmark matrix or formula updated | `PART2_TASKS_ROADMAP.md` §10 | `README.md` |
| Packaging or submission script updated | `PART2_TASKS_ROADMAP.md` §11 | `README.md` |

---

## 11. Integration and Regression Procedure

Whenever a phase feature branch is merged into `develop`:
1. **Clean Workspace:** Fresh clone or `git clean -fdx` in a scratch directory.
2. **All-Variant Build:**
   ```bash
   make -C Part1
   make -C Part2_Case1
   make -C Part2_Case2
   make -C Part2_Case2_Enhanced
   ```
   *(Only targets for completed phases will build; others skipped if not yet reached).*
3. **Preceding Test Suite Execution:** Re-run all existing test scripts in `tests/`.
4. If any regression is detected, the merge is rolled back immediately, and the phase owner resolves the defect on their feature branch.

---

## 12. Final Submission Packaging Procedure

On 16 October 2026, Yug Malaviya executes the final packaging checklist derived from Tier 1 Assignment §Submission:

### 12.1 Automated Packaging Verification (`scripts/package_submission.sh`)
```bash
# 1. Run master correctness regression suite
bash scripts/test_correctness.sh

# 2. Execute automated packaging script
bash scripts/package_submission.sh

# 3. Verify clean extraction in sandbox
mkdir -p /tmp/sandbox_test
tar -xzvf FinalSubmission_Group_071_072.tar.gz -C /tmp/sandbox_test
cd /tmp/sandbox_test/FinalSubmission_Group_071_072

# 4. Confirm exact directory contents
ls -la
# Must contain: FinalReport_Group_071_072.pdf, README.md, Part1/, Part2_Case1/,
# Part2_Case2/, Part2_Case2_Enhanced/, Results/

# 5. Build all variants in sandbox
make -C Part1
make -C Part2_Case1
make -C Part2_Case2
make -C Part2_Case2_Enhanced

# 6. Verify binaries exist
# Confirm exactly 'server' and 'client' exist in each case directory
```

### 12.2 Submission Sign-off
Upon successful sandbox execution, all six team members inspect the archive, sign off on the consolidated report cover page, and Dharmraj uploads `FinalSubmission_Group_071_072.tar.gz` to the university course portal before the 23:59 IST deadline.
