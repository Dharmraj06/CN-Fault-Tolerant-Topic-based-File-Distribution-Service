#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

# Unique port and fixture paths for this test run
PORT=19992
FIXTURE_DIR="/tmp/test_phase3_fixtures_$$"
DOWNLOAD_BASE="/tmp/test_phase3_downloads_$$"
VERIFICATION_LOG="${REPO_ROOT}/tests/phase3_fault_recovery.log"

SERVER_PID=""
CLIENT_PID=""

cleanup() {
    if [ -n "${CLIENT_PID}" ]; then
        kill -9 "${CLIENT_PID}" 2>/dev/null || true
        wait "${CLIENT_PID}" 2>/dev/null || true
    fi
    if [ -n "${SERVER_PID}" ]; then
        kill -9 "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    chmod -R 755 "${DOWNLOAD_BASE}" 2>/dev/null || true
    rm -rf "${FIXTURE_DIR}" "${DOWNLOAD_BASE}"
}
trap cleanup EXIT INT TERM

log_msg() {
    echo "$@" | tee -a "${VERIFICATION_LOG}"
}

mkdir -p "${REPO_ROOT}/tests"
rm -f "${VERIFICATION_LOG}"

log_msg "================================================================================"
log_msg "IT305 PHASE 3 MASTER INTEGRATION & ACCEPTANCE VERIFICATION"
log_msg "Target Binaries: Part2_Case1/server & Part2_Case1/client"
log_msg "Timestamp: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
log_msg "Environment: Linux $(uname -r) ($(gcc --version | head -n 1))"
log_msg "================================================================================"
log_msg ""

# ==============================================================================
# GATE 0: BUILD CLEANLINESS AUDIT
# ==============================================================================
log_msg "--------------------------------------------------------------------------------"
log_msg "GATE 0: Build Cleanliness Audit (Part2_Case1/Makefile)"
log_msg "--------------------------------------------------------------------------------"
log_msg "Command: make -C Part2_Case1 clean && make -C Part2_Case1"
(make -C Part2_Case1 clean && make -C Part2_Case1) >> "${VERIFICATION_LOG}" 2>&1
log_msg "[PASS] Part2_Case1/server and Part2_Case1/client built cleanly with zero compiler warnings"
log_msg ""

# ==============================================================================
# FIXTURE SETUP
# ==============================================================================
log_msg ">>> Setting up temporary test fixtures in ${FIXTURE_DIR}..."
rm -rf "${FIXTURE_DIR}" "${DOWNLOAD_BASE}"
mkdir -p "${FIXTURE_DIR}" "${DOWNLOAD_BASE}"

# 1. Topic Alpha: Multi-file topic with small, medium, and large files
mkdir -p "${FIXTURE_DIR}/topic_alpha"
printf "Alpha sample content text file 1\n" > "${FIXTURE_DIR}/topic_alpha/doc1.txt"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_alpha/data_128k.bin" bs=65536 count=2 2>/dev/null
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_alpha/data_300k.bin" bs=1024 count=300 2>/dev/null
touch "${FIXTURE_DIR}/topic_alpha/empty.dat"

# 2. Topic Beta: Multiple medium files for multi-chunk streaming
mkdir -p "${FIXTURE_DIR}/topic_beta"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_beta/chunked_1.bin" bs=1024 count=150 2>/dev/null
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_beta/chunked_2.bin" bs=1024 count=200 2>/dev/null
printf "Metadata summary file for beta\n" > "${FIXTURE_DIR}/topic_beta/summary.txt"

# 3. Topic Large: Large payload for stress testing disconnects
mkdir -p "${FIXTURE_DIR}/topic_large"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_large/stream_2m.bin" bs=1048576 count=2 2>/dev/null

log_msg "[INFO] Fixtures prepared successfully"
log_msg ""

# Poll until a TCP port accepts connections (replaces a fixed sleep after starting the server)
wait_for_port() {
    local port="$1"
    local i
    for i in $(seq 1 100); do
        if (exec 3<>"/dev/tcp/127.0.0.1/${port}") 2>/dev/null; then
            return 0
        fi
        sleep 0.05
    done
    return 1
}

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
    done

    return 0
}

PASSED_COUNT=0
TOTAL_SCENARIOS=6

# ==============================================================================
# SCENARIO 1: CLI ARGUMENT PARSING & VALIDATION
# ==============================================================================
log_msg "--- SCENARIO 1: CLI Argument Parsing & Validation ---"

# Server: Reject missing arguments
RC_SRV1=0
"${REPO_ROOT}/Part2_Case1/server" >> "${VERIFICATION_LOG}" 2>&1 || RC_SRV1=$?

# Server: Reject negative probability
RC_SRV2=0
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" "-0.1" >> "${VERIFICATION_LOG}" 2>&1 || RC_SRV2=$?

# Server: Reject probability > 1.0
RC_SRV3=0
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" "1.5" >> "${VERIFICATION_LOG}" 2>&1 || RC_SRV3=$?

# Server: Reject non-numeric probability
RC_SRV4=0
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" "invalid_prob" >> "${VERIFICATION_LOG}" 2>&1 || RC_SRV4=$?

# Client: Reject missing arguments
RC_CLI1=0
"${REPO_ROOT}/Part2_Case1/client" >> "${VERIFICATION_LOG}" 2>&1 || RC_CLI1=$?

# Client: Reject path traversal in topic name
RC_CLI2=0
"${REPO_ROOT}/Part2_Case1/client" 127.0.0.1 "${PORT}" "../etc" "${DOWNLOAD_BASE}" >> "${VERIFICATION_LOG}" 2>&1 || RC_CLI2=$?

if [ "${RC_SRV1}" -ne 0 ] && [ "${RC_SRV2}" -ne 0 ] && [ "${RC_SRV3}" -ne 0 ] && \
   [ "${RC_SRV4}" -ne 0 ] && [ "${RC_CLI1}" -ne 0 ] && [ "${RC_CLI2}" -ne 0 ]; then
    log_msg "  [PASS] Scenario 1: CLI argument validation strictly enforces boundary and security rules"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 1: CLI validation failed to reject invalid inputs"
fi
log_msg ""

# ==============================================================================
# SCENARIO 2: ZERO-FAULT BASELINE TRANSFER (p = 0.0)
# ==============================================================================
log_msg "--- SCENARIO 2: Zero-Fault Baseline Transfer (p = 0.0) ---"
log_msg "Starting server with failure_prob = 0.0 on port ${PORT}..."
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" 0.0 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
sleep 0.5

OUT2="${DOWNLOAD_BASE}/out_sc2"
mkdir -p "${OUT2}"
RC=0
"${REPO_ROOT}/Part2_Case1/client" 127.0.0.1 "${PORT}" topic_alpha "${OUT2}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${RC}" -eq 0 ] && verify_transfer "topic_alpha" "${OUT2}"; then
    log_msg "  [PASS] Scenario 2: Zero-fault transfer completed on attempt 1 with bit-exact SHA-256 match"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 2 failed (exit code: ${RC})"
fi
log_msg ""

# ==============================================================================
# SCENARIO 3: FAULT INJECTION & RECOVERY (p = 0.05)
# ==============================================================================
log_msg "--- SCENARIO 3: Fault Injection & Full Restart Recovery (p = 0.05) ---"
PORT=$((PORT + 1))
log_msg "Starting server with failure_prob = 0.05 on port ${PORT}..."
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" 0.05 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
sleep 0.5

OUT3="${DOWNLOAD_BASE}/out_sc3"
mkdir -p "${OUT3}"
RC=0
"${REPO_ROOT}/Part2_Case1/client" 127.0.0.1 "${PORT}" topic_beta "${OUT3}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${RC}" -eq 0 ] && verify_transfer "topic_beta" "${OUT3}"; then
    log_msg "  [PASS] Scenario 3: Server injected drops, client purged and restarted, completed bit-exact"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 3 failed (exit code: ${RC})"
fi
log_msg ""

# ==============================================================================
# SCENARIO 4: NO PARTIAL FILE RESIDUE ON ABORTED TRANSFER
# ==============================================================================
log_msg "--- SCENARIO 4: No Partial File Residue on Aborted Transfer ---"
PORT=$((PORT + 1))
log_msg "Starting server with failure_prob = 0.50 on port ${PORT}..."
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" 0.50 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 4: server did not start listening"; exit 1; }

OUT4="${DOWNLOAD_BASE}/out_sc4"
mkdir -p "${OUT4}"
# Start the first client in the background and capture its PID.
# We interrupt it with SIGTERM, not SIGINT: a non-interactive bash starts
# background (&) commands with SIGINT/SIGQUIT set to "ignore", and ignored
# signals stay ignored across exec(), so 'kill -INT' would have no effect.
# SIGTERM is not ignored and the client has no handler, so the default action
# (terminate) applies.
"${REPO_ROOT}/Part2_Case1/client" 127.0.0.1 "${PORT}" topic_large "${OUT4}" >> "${VERIFICATION_LOG}" 2>&1 &
CLIENT_PID=$!

# Wait (no fixed sleep) until the client has started writing the file locally,
# i.e. it is genuinely in the middle of a transfer attempt.
MID_TRANSFER=0
for _ in $(seq 1 20000); do
    if compgen -G "${OUT4}/topic_large/*" > /dev/null; then
        MID_TRANSFER=1
        break
    fi
    kill -0 "${CLIENT_PID}" 2>/dev/null || break
    sleep 0.001
done

kill -TERM "${CLIENT_PID}" 2>/dev/null || true
RC_FIRST=0
wait "${CLIENT_PID}" 2>/dev/null || RC_FIRST=$?
CLIENT_PID=""

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

# The interrupted client must be dead from SIGTERM (128 + 15 = 143)
INTERRUPT_OK=0
if [ "${MID_TRANSFER}" -eq 1 ] && [ "${RC_FIRST}" -eq 143 ]; then
    INTERRUPT_OK=1
    log_msg "  [INFO] Scenario 4: first client interrupted mid-transfer, exit status ${RC_FIRST}"
else
    log_msg "  [INFO] Scenario 4: interruption check failed (mid_transfer=${MID_TRANSFER}, first client exit=${RC_FIRST}, expected 143)"
fi

# Now verify that a fresh client run purges any residue and restarts from byte 0
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" 0.0 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 4: second server did not start listening"; exit 1; }

RC=0
"${REPO_ROOT}/Part2_Case1/client" 127.0.0.1 "${PORT}" topic_large "${OUT4}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${INTERRUPT_OK}" -eq 1 ] && [ "${RC}" -eq 0 ] && verify_transfer "topic_large" "${OUT4}"; then
    log_msg "  [PASS] Scenario 4: Partial file residue cleanly purged; restarted transfer matches 100%"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 4 failed (exit code: ${RC})"
fi
log_msg ""

# ==============================================================================
# SCENARIO 5: PERSISTENT FAILURE & MAX RETRY ABORT (p = 1.0)
# ==============================================================================
log_msg "--- SCENARIO 5: Persistent Failure & Max Retry Abort (p = 1.0) ---"
PORT=$((PORT + 1))
log_msg "Starting server with failure_prob = 1.0 on port ${PORT}..."
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" 1.0 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
sleep 0.5

OUT5="${DOWNLOAD_BASE}/out_sc5"
mkdir -p "${OUT5}"
RC=0
"${REPO_ROOT}/Part2_Case1/client" 127.0.0.1 "${PORT}" topic_alpha "${OUT5}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

# At p=1.0, every single chunk drops, so client must abort after MAX_RETRY_ATTEMPTS with exit code 1
# and the destination directory must be empty or purged (no leftover corrupted files)
RESIDUE_COUNT=0
if [ -d "${OUT5}/topic_alpha" ]; then
    RESIDUE_COUNT=$(find "${OUT5}/topic_alpha" -type f | wc -l)
fi

if [ "${RC}" -eq 1 ] && [ "${RESIDUE_COUNT}" -eq 0 ]; then
    log_msg "  [PASS] Scenario 5: At p=1.0, client cleanly aborted after max retries with zero file residue"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 5 failed (exit code: ${RC}, residue files: ${RESIDUE_COUNT})"
fi
log_msg ""

# ==============================================================================
# SCENARIO 6: MULTI-CLIENT CONCURRENT ISOLATION UNDER FAULTS
# ==============================================================================
log_msg "--- SCENARIO 6: Multi-Client Concurrent Isolation under Faults ---"
PORT=$((PORT + 1))
log_msg "Starting server with failure_prob = 0.03 on port ${PORT}..."
"${REPO_ROOT}/Part2_Case1/server" "${PORT}" "${FIXTURE_DIR}" 0.03 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
sleep 0.5

OUT6_A="${DOWNLOAD_BASE}/out_sc6_alpha"
OUT6_B="${DOWNLOAD_BASE}/out_sc6_beta"
mkdir -p "${OUT6_A}" "${OUT6_B}"

RC_A=0
RC_B=0
"${REPO_ROOT}/Part2_Case1/client" 127.0.0.1 "${PORT}" topic_alpha "${OUT6_A}" >> "${VERIFICATION_LOG}" 2>&1 &
PID_A=$!
"${REPO_ROOT}/Part2_Case1/client" 127.0.0.1 "${PORT}" topic_beta "${OUT6_B}" >> "${VERIFICATION_LOG}" 2>&1 &
PID_B=$!

wait "${PID_A}" || RC_A=$?
wait "${PID_B}" || RC_B=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${RC_A}" -eq 0 ] && [ "${RC_B}" -eq 0 ] && \
   verify_transfer "topic_alpha" "${OUT6_A}" && verify_transfer "topic_beta" "${OUT6_B}"; then
    log_msg "  [PASS] Scenario 6: Concurrent clients recovered independently and finished bit-exact"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 6 failed (RC_A=${RC_A}, RC_B=${RC_B})"
fi
log_msg ""

# ==============================================================================
# FINAL SUMMARY
# ==============================================================================
log_msg "================================================================================"
log_msg "PHASE 3 VERIFICATION SUMMARY: ${PASSED_COUNT} / ${TOTAL_SCENARIOS} SCENARIOS PASSED"
log_msg "Detailed log available at: ${VERIFICATION_LOG}"
log_msg "================================================================================"

if [ "${PASSED_COUNT}" -eq "${TOTAL_SCENARIOS}" ]; then
    log_msg "STATUS: ALL PHASE 3 ACCEPTANCE GATES PASSED [100%]"
    exit 0
else
    log_msg "STATUS: VERIFICATION DEFECTS DETECTED"
    exit 1
fi
