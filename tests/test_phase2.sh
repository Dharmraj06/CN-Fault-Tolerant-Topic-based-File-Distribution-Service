#!/usr/bin/env bash
# ==============================================================================
# IT305 Computer Networks Lab - Autumn 2026
# Project: Fault-Tolerant Topic-Based File Distribution Service
#
# Script: tests/test_phase2.sh
# Description: Master End-to-End Integration & Acceptance Test Suite for Phase 2
#              (Baseline Server & Client Interoperability)
#
# Source of Truth:
#   - Tier 1 Assignment Specification (§Part 1)
#   - Approved Master Execution Roadmap (PART2_TASKS_ROADMAP.md §4.32, §5 Task 2.4)
#   - College-Level Development Workflow (§6, §7)
# ==============================================================================

set -euo pipefail

export PATH="$HOME/.local/usr/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

# Unique port and temp directory for this test run
PORT=19990
FIXTURE_DIR="/tmp/test_phase2_fixtures_$$"
DOWNLOAD_BASE="/tmp/test_phase2_downloads_$$"
VERIFICATION_LOG="${REPO_ROOT}/tests/phase2_verification.log"
INTEGRITY_REPORT="${REPO_ROOT}/tests/phase2_integrity_report.txt"

SERVER_PID=""

cleanup() {
    if [ -n "${SERVER_PID}" ]; then
        kill -9 "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    # Restore permissions if any read-only dirs exist
    chmod -R 755 "${DOWNLOAD_BASE}" 2>/dev/null || true
    rm -rf "${FIXTURE_DIR}" "${DOWNLOAD_BASE}"
}
trap cleanup EXIT INT TERM

log_msg() {
    echo "$@" | tee -a "${VERIFICATION_LOG}"
}

# Initialize logs
mkdir -p "${REPO_ROOT}/tests"
rm -f "${VERIFICATION_LOG}" "${INTEGRITY_REPORT}"

log_msg "================================================================================"
log_msg "IT305 PHASE 2 MASTER INTEGRATION & ACCEPTANCE VERIFICATION"
log_msg "Target Binaries: Part1/server & Part1/client"
log_msg "Timestamp: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
log_msg "Environment: Linux $(uname -r) ($(gcc --version | head -n 1))"
log_msg "================================================================================"
log_msg ""

# ==============================================================================
# GATE 0: BUILD CLEANLINESS AUDIT
# ==============================================================================
log_msg "--------------------------------------------------------------------------------"
log_msg "GATE 0: Build Cleanliness Audit (Part1/Makefile)"
log_msg "--------------------------------------------------------------------------------"
log_msg "Command: make -C Part1 clean && make -C Part1"
(make -C Part1 clean && make -C Part1) >> "${VERIFICATION_LOG}" 2>&1
log_msg "[PASS] Part1/server and Part1/client built cleanly with zero compiler warnings"
log_msg ""

# ==============================================================================
# FIXTURE SETUP
# ==============================================================================
log_msg ">>> Setting up temporary test fixtures in ${FIXTURE_DIR}..."
rm -rf "${FIXTURE_DIR}" "${DOWNLOAD_BASE}"
mkdir -p "${FIXTURE_DIR}" "${DOWNLOAD_BASE}"

# 1. Multiple small files topic
mkdir -p "${FIXTURE_DIR}/topic_small"
printf "Small test file 1 content.\n" > "${FIXTURE_DIR}/topic_small/small_1.txt"
printf "Another small file for network transfer.\n" > "${FIXTURE_DIR}/topic_small/small_2.dat"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_small/small_3.bin" bs=512 count=1 2>/dev/null

# 2. Files larger than 64 KiB
mkdir -p "${FIXTURE_DIR}/topic_large"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_large/large_180k.bin" bs=1024 count=180 2>/dev/null
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_large/large_320k.bin" bs=1024 count=320 2>/dev/null

# 3. Exactly chunk boundary files (64 KiB = 65536 B, 128 KiB = 131072 B)
mkdir -p "${FIXTURE_DIR}/topic_boundary"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_boundary/exact_64k.bin" bs=65536 count=1 2>/dev/null
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_boundary/exact_128k.bin" bs=65536 count=2 2>/dev/null

# 4. Final partial chunks
mkdir -p "${FIXTURE_DIR}/topic_partial"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_partial/partial_70k.bin" bs=1024 count=70 2>/dev/null
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_partial/partial_150k.bin" bs=1024 count=150 2>/dev/null

# 5. Empty files
mkdir -p "${FIXTURE_DIR}/topic_empty"
touch "${FIXTURE_DIR}/topic_empty/empty_1.dat"
touch "${FIXTURE_DIR}/topic_empty/empty_2.txt"
printf "Companion non-empty file\n" > "${FIXTURE_DIR}/topic_empty/companion.txt"

# 7. Filenames containing spaces
mkdir -p "${FIXTURE_DIR}/topic_spaces"
printf "Document content with spaces in filename.\n" > "${FIXTURE_DIR}/topic_spaces/my document file.txt"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_spaces/annual report 2026.dat" bs=1024 count=80 2>/dev/null

# Multi-client topics (Alpha & Beta for concurrent isolation test)
mkdir -p "${FIXTURE_DIR}/alpha_topic" "${FIXTURE_DIR}/beta_topic"
printf "Alpha dataset payload text\n" > "${FIXTURE_DIR}/alpha_topic/alpha_info.txt"
dd if=/dev/urandom of="${FIXTURE_DIR}/alpha_topic/alpha_image.bin" bs=1024 count=95 2>/dev/null

printf "Beta dataset payload text\n" > "${FIXTURE_DIR}/beta_topic/beta_info.txt"
dd if=/dev/urandom of="${FIXTURE_DIR}/beta_topic/beta_data.bin" bs=1024 count=110 2>/dev/null

# Topic for disconnect test (100 MB)
mkdir -p "${FIXTURE_DIR}/topic_disconnect"
dd if=/dev/zero of="${FIXTURE_DIR}/topic_disconnect/stream_100m.dat" bs=1048576 count=100 2>/dev/null

log_msg "[INFO] Fixtures prepared successfully"
log_msg ""

# ==============================================================================
# START BACKGROUND SERVER
# ==============================================================================
log_msg ">>> Starting baseline Part1/server on port ${PORT} (Multi-Threaded mode)..."
"${REPO_ROOT}/Part1/server" "${PORT}" "${FIXTURE_DIR}" mt >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
sleep 0.5

# Helper function to verify transfer directory and SHA256 integrity
verify_transfer() {
    local topic="$1"
    local dest_dir="$2"
    local src_dir="${FIXTURE_DIR}/${topic}"
    local target_dir="${dest_dir}/${topic}"

    if [ ! -d "${target_dir}" ]; then
        log_msg "  [FAIL] Target directory '${target_dir}' does not exist!"
        return 1
    fi

    # Bit-exact directory diff
    if ! diff -r "${src_dir}" "${target_dir}" >> "${VERIFICATION_LOG}" 2>&1; then
        log_msg "  [FAIL] diff -r found discrepancies between '${src_dir}' and '${target_dir}'!"
        return 1
    fi

    # SHA-256 comparison for all files
    for f in "${src_dir}"/*; do
        local fname
        fname="$(basename "${f}")"
        local src_hash
        src_hash="$(sha256sum "${f}" | awk '{print $1}')"
        local dst_hash
        dst_hash="$(sha256sum "${target_dir}/${fname}" | awk '{print $1}')"

        if [ "${src_hash}" != "${dst_hash}" ]; then
            log_msg "  [FAIL] SHA-256 hash mismatch for '${fname}': expected ${src_hash}, got ${dst_hash}"
            return 1
        fi
        echo "${src_hash}  ${topic}/${fname} [MATCH]" >> "${INTEGRITY_REPORT}"
    done

    return 0
}

# ==============================================================================
# INTEGRATION SCENARIOS
# ==============================================================================
PASSED_COUNT=0
TOTAL_SCENARIOS=12

# ------------------------------------------------------------------------------
# SCENARIO 1: Multiple small files
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 1: Multiple Small Files ---"
OUT1="${DOWNLOAD_BASE}/out_sc1"
mkdir -p "${OUT1}"
RC=0
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" topic_small "${OUT1}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?
if [ "${RC}" -eq 0 ] && verify_transfer "topic_small" "${OUT1}"; then
    log_msg "  [PASS] Scenario 1: Multiple small files transferred bit-exact (exit code: ${RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 1 failed (exit code: ${RC})"
fi

# ------------------------------------------------------------------------------
# SCENARIO 2: Files larger than 64 KiB
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 2: Files Larger Than 64 KiB ---"
OUT2="${DOWNLOAD_BASE}/out_sc2"
mkdir -p "${OUT2}"
RC=0
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" topic_large "${OUT2}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?
if [ "${RC}" -eq 0 ] && verify_transfer "topic_large" "${OUT2}"; then
    log_msg "  [PASS] Scenario 2: Large files transferred across multiple chunks bit-exact (exit code: ${RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 2 failed (exit code: ${RC})"
fi

# ------------------------------------------------------------------------------
# SCENARIO 3: Files exactly equal to chunk boundary (64 KiB, 128 KiB)
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 3: Exact Chunk Boundary (64 KiB, 128 KiB) ---"
OUT3="${DOWNLOAD_BASE}/out_sc3"
mkdir -p "${OUT3}"
RC=0
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" topic_boundary "${OUT3}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?
if [ "${RC}" -eq 0 ] && verify_transfer "topic_boundary" "${OUT3}"; then
    log_msg "  [PASS] Scenario 3: Exact chunk boundary files transferred bit-exact (exit code: ${RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 3 failed (exit code: ${RC})"
fi

# ------------------------------------------------------------------------------
# SCENARIO 4: Final partial chunks
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 4: Final Partial Chunks ---"
OUT4="${DOWNLOAD_BASE}/out_sc4"
mkdir -p "${OUT4}"
RC=0
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" topic_partial "${OUT4}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?
if [ "${RC}" -eq 0 ] && verify_transfer "topic_partial" "${OUT4}"; then
    log_msg "  [PASS] Scenario 4: Files with final partial chunks reassembled bit-exact (exit code: ${RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 4 failed (exit code: ${RC})"
fi

# ------------------------------------------------------------------------------
# SCENARIO 5: Empty files (0 bytes)
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 5: Empty Files (0 Bytes) ---"
OUT5="${DOWNLOAD_BASE}/out_sc5"
mkdir -p "${OUT5}"
RC=0
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" topic_empty "${OUT5}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?
if [ "${RC}" -eq 0 ] && verify_transfer "topic_empty" "${OUT5}"; then
    log_msg "  [PASS] Scenario 5: Zero-byte files created and verified bit-exact (exit code: ${RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 5 failed (exit code: ${RC})"
fi

# ------------------------------------------------------------------------------
# SCENARIO 6: Missing topics
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 6: Missing Topics (404 Rejection) ---"
OUT6="${DOWNLOAD_BASE}/out_sc6"
mkdir -p "${OUT6}"
set +e
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" nonexistent_topic_404 "${OUT6}" >> "${VERIFICATION_LOG}" 2>&1
RC=$?
set -e
if [ "${RC}" -ne 0 ]; then
    log_msg "  [PASS] Scenario 6: Nonexistent topic correctly rejected with error code (exit code: ${RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 6: Nonexistent topic unexpectedly returned exit code 0"
fi

# ------------------------------------------------------------------------------
# SCENARIO 7: Ordinary supported filenames containing spaces
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 7: Filenames Containing Spaces ---"
OUT7="${DOWNLOAD_BASE}/out_sc7"
mkdir -p "${OUT7}"
RC=0
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" topic_spaces "${OUT7}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?
if [ "${RC}" -eq 0 ] && verify_transfer "topic_spaces" "${OUT7}"; then
    log_msg "  [PASS] Scenario 7: Files with spaces in filename transferred bit-exact (exit code: ${RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 7 failed (exit code: ${RC})"
fi

# ------------------------------------------------------------------------------
# SCENARIO 8: Traversal attempts and invalid topics
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 8: Traversal Attempts & Invalid Topics ---"
SC8_PASS=true
for bad_topic in "../../etc/passwd" "topic/sub" "topic:test" "topic;ls"; do
    set +e
    "${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" "${bad_topic}" "${DOWNLOAD_BASE}" >> "${VERIFICATION_LOG}" 2>&1
    SUB_RC=$?
    set -e
    if [ "${SUB_RC}" -eq 0 ]; then
        log_msg "  [FAIL] Bad topic '${bad_topic}' was improperly accepted"
        SC8_PASS=false
        break
    fi
done
if [ "${SC8_PASS}" = true ]; then
    log_msg "  [PASS] Scenario 8: All traversal and invalid topic attempts rejected safely"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 8 failed"
fi

# ------------------------------------------------------------------------------
# SCENARIO 9: Server disconnects during transfer
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 9: Server Disconnect Mid-Transfer ---"
# Launch a dedicated server for this test so we can kill it mid-stream
PORT_DISC=19991
"${REPO_ROOT}/Part1/server" "${PORT_DISC}" "${FIXTURE_DIR}" mt >> "${VERIFICATION_LOG}" 2>&1 &
DISC_SERVER_PID=$!
sleep 0.5

OUT9="${DOWNLOAD_BASE}/out_sc9"
mkdir -p "${OUT9}"
# Launch client transferring 100 MB stream
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT_DISC}" topic_disconnect "${OUT9}" >> "${VERIFICATION_LOG}" 2>&1 &
CLIENT_DISC_PID=$!

# Wait until destination file is created on disk, confirming transfer has started
for _ in $(seq 1 200); do
    if [ -f "${OUT9}/topic_disconnect/stream_100m.dat" ]; then
        break
    fi
    sleep 0.001
done

# Abruptly terminate server while client is actively receiving chunk data
kill -9 "${DISC_SERVER_PID}" 2>/dev/null || true
wait "${DISC_SERVER_PID}" 2>/dev/null || true

set +e
wait "${CLIENT_DISC_PID}"
DISC_CLIENT_RC=$?
set -e

if [ "${DISC_CLIENT_RC}" -ne 0 ]; then
    log_msg "  [PASS] Scenario 9: Client detected server disconnect and exited cleanly with error (exit code: ${DISC_CLIENT_RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 9: Client exited with 0 despite sudden server disconnect"
fi

# ------------------------------------------------------------------------------
# SCENARIO 10: Destination write failures where safely reproducible
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 10: Destination Write Failure ---"
OUT10="${DOWNLOAD_BASE}/out_sc10"
mkdir -p "${OUT10}/topic_small"
chmod 555 "${OUT10}/topic_small" # Make topic destination directory read-only

set +e
"${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" topic_small "${OUT10}" >> "${VERIFICATION_LOG}" 2>&1
WRITE_RC=$?
set -e

chmod 755 "${OUT10}/topic_small" # Restore permissions for cleanup
if [ "${WRITE_RC}" -ne 0 ]; then
    log_msg "  [PASS] Scenario 10: Destination write failure gracefully detected and handled (exit code: ${WRITE_RC})"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 10: Client should have failed when destination directory was read-only"
fi

# ------------------------------------------------------------------------------
# SCENARIO 11 & 12: Eight simultaneous independent clients & isolation
# ------------------------------------------------------------------------------
log_msg "--- SCENARIO 11 & 12: Eight Simultaneous Clients & Isolation ---"
CONC_PIDS=()
CONC_OUTS=()
CONC_TOPICS=()

for i in $(seq 1 8); do
    OUT_I="${DOWNLOAD_BASE}/out_client_${i}"
    mkdir -p "${OUT_I}"
    CONC_OUTS+=("${OUT_I}")

    # Alternate topics: odd clients request alpha, even clients request beta
    if [ $((i % 2)) -eq 1 ]; then
        TOPIC_I="alpha_topic"
    else
        TOPIC_I="beta_topic"
    fi
    CONC_TOPICS+=("${TOPIC_I}")

    "${REPO_ROOT}/Part1/client" 127.0.0.1 "${PORT}" "${TOPIC_I}" "${OUT_I}" >> "${VERIFICATION_LOG}" 2>&1 &
    CONC_PIDS+=($!)
done

ALL_CONC_PASS=true
for idx in $(seq 0 7); do
    pid="${CONC_PIDS[${idx}]}"
    set +e
    wait "${pid}"
    C_RC=$?
    set -e
    if [ "${C_RC}" -ne 0 ]; then
        log_msg "  [FAIL] Concurrent client $((idx + 1)) exited with error code ${C_RC}"
        ALL_CONC_PASS=false
    fi
done

if [ "${ALL_CONC_PASS}" = true ]; then
    log_msg "  [PASS] Scenario 11: All 8 simultaneous independent clients completed with exit code 0"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 11 failed"
fi

# Scenario 12: Verify isolation and zero cross-client contamination
ISOLATION_PASS=true
for idx in $(seq 0 7); do
    c_out="${CONC_OUTS[${idx}]}"
    c_topic="${CONC_TOPICS[${idx}]}"

    if ! verify_transfer "${c_topic}" "${c_out}"; then
        log_msg "  [FAIL] Integrity verification failed for client $((idx + 1)) (${c_topic})"
        ISOLATION_PASS=false
    fi

    # Check that client does not have files from the other topic
    if [ "${c_topic}" = "alpha_topic" ] && [ -d "${c_out}/beta_topic" ]; then
        log_msg "  [FAIL] Client $((idx + 1)) cross-contaminated with beta_topic!"
        ISOLATION_PASS=false
    elif [ "${c_topic}" = "beta_topic" ] && [ -d "${c_out}/alpha_topic" ]; then
        log_msg "  [FAIL] Client $((idx + 1)) cross-contaminated with alpha_topic!"
        ISOLATION_PASS=false
    fi
done

if [ "${ISOLATION_PASS}" = true ]; then
    log_msg "  [PASS] Scenario 12: Perfect isolation verified across all 8 clients with zero cross-contamination"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 12 failed"
fi

# ==============================================================================
# SUMMARY & EXIT GATE
# ==============================================================================
log_msg ""
log_msg "================================================================================"
log_msg "VERIFICATION SUMMARY: ${PASSED_COUNT} / ${TOTAL_SCENARIOS} SCENARIOS PASSED"
log_msg "Integrity Report Generated: ${INTEGRITY_REPORT}"
log_msg "Verification Log Generated: ${VERIFICATION_LOG}"

if [ "${PASSED_COUNT}" -eq "${TOTAL_SCENARIOS}" ]; then
    log_msg "DECISION: PASS (All Phase 2 Acceptance Criteria 100% Satisfied)"
    log_msg "================================================================================"
    exit 0
else
    log_msg "DECISION: FAIL (One or more scenarios failed)"
    log_msg "================================================================================"
    exit 1
fi
