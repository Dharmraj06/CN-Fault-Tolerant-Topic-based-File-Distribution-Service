# FINAL SUBMISSION CHECKLIST & PACKAGING SPECIFICATION
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Submission Package**: `FinalSubmission_Group_071_072.zip`  
**Official Course Deadline**: 16 October 2026 (23:59 IST)  
**Internal Target Submission**: 16 October 2026 (21:00 IST)  

---

## 1. Required Directory & File Structure

The final submitted archive must extract to a single top-level directory named `FinalSubmission_Group_071_072/` with the exact structure below. Deviations will cause automated grader failure:

```
FinalSubmission_Group_071_072/
├── FinalReport_Group_071_072.pdf  <-- Consolidated PDF report
├── README.md                      <-- Build/run instructions & fault mechanism details
├── Part1/
│   ├── server.c
│   ├── client.c
│   ├── Makefile                   <-- 'make' produces 'server' and 'client'
│   └── [supporting .c and .h files]
├── Part2_Case1/
│   ├── server.c
│   ├── client.c
│   ├── Makefile                   <-- 'make' produces 'server' and 'client'
│   └── [supporting .c and .h files]
├── Part2_Case2/
│   ├── server.c
│   ├── client.c
│   ├── Makefile                   <-- 'make' produces 'server' and 'client'
│   └── [supporting .c and .h files]
├── Part2_Case2_Enhanced/
│   ├── server.c
│   ├── client.c
│   ├── Makefile                   <-- 'make' produces 'server' and 'client'
│   └── [supporting .c and .h files]
└── Results/
    ├── part1_benchmark.csv
    ├── part2_case1_vs_case2.csv
    ├── part2_enhanced_scaling.csv
    ├── Part1_throughput_vs_clients.png
    ├── Part2_Case1_vs_Case2_throughput.png
    ├── Part2_Case1_vs_Case2_overhead.png
    └── Part2_Case2_vs_Enhanced_speedup.png
```

---

## 2. Strict Automated Grader Compliance Checklist

- [ ] **Exact Executable Names**:
  - `Part1/` produces `./server` and `./client`
  - `Part2_Case1/` produces `./server` and `./client`
  - `Part2_Case2/` produces `./server` and `./client`
  - `Part2_Case2_Enhanced/` produces `./server` and `./client`
- [ ] **Exact Server Invocations**:
  - Part 1: `./server <port> <topics_root_dir>` (exactly 2 positional arguments)
  - Part 2 (all cases): `./server <port> <topics_root_dir> <failure probability>` (exactly 3 positional arguments)
- [ ] **Exact Client Invocation**:
  - All parts: `./client <server_ip> <server_port> <topic> <output_dir>` (exactly 4 positional arguments)
  - Enhanced case: Optional 5th trailing argument `./client <server_ip> <server_port> <topic> <output_dir> [num_conns]` with sensible default of 1 if omitted.
- [ ] **Client Output Path Layout**:
  - Downloaded files must reside strictly under `<output_dir>/<topic>/`.
- [ ] **No Hardcoded Environment Values**:
  - Zero hardcoded IP addresses, ports, topic names, or file paths.
- [ ] **Clean Build**:
  - `make clean && make` succeeds with 0 compilation errors and 0 warnings under `-Wall -Wextra`.
  - Zero external third-party library dependencies (pure POSIX standard C).
- [ ] **Non-Zero Exit Status on Error**:
  - Both server and client exit with status `> 0` on invalid arguments, path traversal, or unrecoverable network failure.

---

## 3. Final Report Content Requirements

`FinalReport_Group_071_072.pdf` must contain:

1. **Cover Page**:
   - Course Name: IT305 Computer Networks Lab (Autumn 2026)
   - Groups: Group 071 + Group 072
   - Complete list of all 6 members with Full Name, Student ID, and an accurate 1-line individual contribution statement:
     - *Jay Vaghela (202401237)*: Designed binary length framing, topic directory manager, and Part 1 server core.
     - *Dharmraj Vaghela (202401236)*: Implemented in-memory session table, checkpoint commit engine, and Case 2 server.
     - *Vivek Dodiya (202401253)*: Built bounded chunk file streaming, atomic disk writer, and Part 1/Case 1 clients.
     - *Atharv Trivedi (202401230)*: Engineered fault injection subsystem, Case 1 server, and Case 2 client resume loop.
     - *Shlok Topaliya (202401229)*: Implemented Case 2 Enhanced parallel TCP work partitioning and logging system.
     - *Yug Malaviya (202401256)*: Developed automated test regression harness, benchmark runner, and report graphs.
2. **Design & Implementation Details**:
   - Technical walkthrough of Part 1 (sequential vs MT), Part 2 Case 1, Case 2, and Case 2 Enhanced.
3. **Design Deviations**:
   - Clear accounting of any differences from `Design_Document_Group_071_072_.pdf`.
4. **Performance Analysis Section**:
   - Embedded tables and high-resolution graphs for throughput, overhead, and speedup.
   - Systems analysis explaining observed trends.

---

## 4. Automated Packaging Script

The team will use `scripts/package_submission.sh` to package and verify the archive:

```bash
#!/usr/bin/env bash
# scripts/package_submission.sh
set -euo pipefail

ARCHIVE_NAME="FinalSubmission_Group_071_072"
STAGING_DIR="/tmp/${ARCHIVE_NAME}"

echo "[1/5] Cleaning and creating staging area..."
rm -rf "$STAGING_DIR" "${ARCHIVE_NAME}.zip"
mkdir -p "$STAGING_DIR"

echo "[2/5] Copying project files..."
cp FinalReport_Group_071_072.pdf "$STAGING_DIR/"
cp README.md "$STAGING_DIR/"
cp -r Part1 "$STAGING_DIR/"
cp -r Part2_Case1 "$STAGING_DIR/"
cp -r Part2_Case2 "$STAGING_DIR/"
cp -r Part2_Case2_Enhanced "$STAGING_DIR/"
cp -r Results "$STAGING_DIR/"

# Remove unnecessary binary/git artifacts
find "$STAGING_DIR" -type f \( -name "*.o" -o -name "server" -o -name "client" -o -name ".DS_Store" \) -delete

echo "[3/5] Testing compilation in clean staging tree..."
for dir in Part1 Part2_Case1 Part2_Case2 Part2_Case2_Enhanced; do
    echo "  Compiling $dir..."
    (cd "$STAGING_DIR/$dir" && make clean && make)
    test -x "$STAGING_DIR/$dir/server"
    test -x "$STAGING_DIR/$dir/client"
    (cd "$STAGING_DIR/$dir" && make clean)
done

echo "[4/5] Creating submission archive ${ARCHIVE_NAME}.zip..."
(cd /tmp && zip -r -q "${ARCHIVE_NAME}.zip" "${ARCHIVE_NAME}")
mv "/tmp/${ARCHIVE_NAME}.zip" ./

echo "[5/5] Verifying archive structure..."
unzip -l "${ARCHIVE_NAME}.zip" | head -n 25

echo "=========================================================="
echo "PACKAGING VERIFIED! Archive ready: ${ARCHIVE_NAME}.zip"
echo "=========================================================="
```
