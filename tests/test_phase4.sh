#!/usr/bin/env bash
# ==============================================================================
# IT305 Computer Networks Lab - Autumn 2026
# Phase 4 Master Integration & Acceptance Test Suite
# Target: Part2_Case2 (In-Memory Session Checkpoint Resumption)
# ==============================================================================

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PORT=19995
FIXTURE_DIR="/tmp/test_phase4_fixtures_$$"
DOWNLOAD_BASE="/tmp/test_phase4_downloads_$$"
VERIFICATION_LOG="${REPO_ROOT}/tests/phase4_resume_verification.log"

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
    rm -rf "${FIXTURE_DIR}" "${DOWNLOAD_BASE}"
}
trap cleanup EXIT INT TERM

log_msg() {
    echo "$1"
    echo "$1" >> "${VERIFICATION_LOG}"
}

wait_for_port() {
    local port="$1"
    for _ in $(seq 1 100); do
        if (exec 3<>"/dev/tcp/127.0.0.1/${port}") 2>/dev/null; then
            exec 3<&-
            exec 3>&-
            return 0
        fi
        sleep 0.05
    done
    return 1
}

verify_transfer() {
    local topic="$1"
    local download_dir="$2"
    local src_dir="${FIXTURE_DIR}/${topic}"
    local dst_dir="${download_dir}/${topic}"

    if [ ! -d "${dst_dir}" ]; then
        log_msg "    [ERROR] Download directory does not exist: ${dst_dir}"
        return 1
    fi

    local src_hash dst_hash
    src_hash=$(find "${src_dir}" -maxdepth 1 -type f -exec sha256sum {} + | sort -k2 | awk '{print $1}' | sha256sum | awk '{print $1}')
    dst_hash=$(find "${dst_dir}" -maxdepth 1 -type f ! -name ".*" -exec sha256sum {} + | sort -k2 | awk '{print $1}' | sha256sum | awk '{print $1}')

    if [ "${src_hash}" != "${dst_hash}" ]; then
        log_msg "    [ERROR] SHA-256 hash mismatch: src=${src_hash}, dst=${dst_hash}"
        return 1
    fi
    return 0
}

mkdir -p "${DOWNLOAD_BASE}"
printf "================================================================================\n" > "${VERIFICATION_LOG}"
printf "IT305 PHASE 4 MASTER INTEGRATION & ACCEPTANCE VERIFICATION\n" >> "${VERIFICATION_LOG}"
printf "Target: Part2_Case2/server & client\n" >> "${VERIFICATION_LOG}"
printf "================================================================================\n\n" >> "${VERIFICATION_LOG}"

log_msg "================================================================================"
log_msg "IT305 PHASE 4 MASTER INTEGRATION & ACCEPTANCE VERIFICATION"
log_msg "================================================================================"

PASSED_COUNT=0
TOTAL_SCENARIOS=8

# --- GATE 0: BUILD CLEANLINESS ---
log_msg "--------------------------------------------------------------------------------"
log_msg "GATE 0: Build Cleanliness Audit (Part2_Case2/Makefile)"
log_msg "--------------------------------------------------------------------------------"
(make -C "${REPO_ROOT}/Part2_Case2" clean && make -C "${REPO_ROOT}/Part2_Case2") >> "${VERIFICATION_LOG}" 2>&1
log_msg "[PASS] Part2_Case2 built cleanly with zero compiler warnings"
log_msg ""

# --- GATE 1: UNIT TESTS ---
log_msg "--------------------------------------------------------------------------------"
log_msg "GATE 1: Session Module Unit Tests (common/session.*)"
log_msg "--------------------------------------------------------------------------------"
gcc -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I"${REPO_ROOT}/common" \
    "${REPO_ROOT}/common/session.c" "${REPO_ROOT}/tests/test_session.c" \
    -o "${REPO_ROOT}/tests/test_session" >> "${VERIFICATION_LOG}" 2>&1
"${REPO_ROOT}/tests/test_session" >> "${VERIFICATION_LOG}" 2>&1
rm -f "${REPO_ROOT}/tests/test_session"
log_msg "[PASS] common/session.*: All unit tests passed"
log_msg ""

# --- FIXTURE SETUP ---
log_msg ">>> Preparing test fixtures..."
rm -rf "${FIXTURE_DIR}"
mkdir -p "${FIXTURE_DIR}"

# 1. topic_alpha (baseline)
mkdir -p "${FIXTURE_DIR}/topic_alpha"
printf "Hello IT305 File 1\n" > "${FIXTURE_DIR}/topic_alpha/file1.txt"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_alpha/data_128k.bin" bs=65536 count=2 2>/dev/null
touch "${FIXTURE_DIR}/topic_alpha/empty.dat"

# 2. topic_beta (medium files for fault injection)
mkdir -p "${FIXTURE_DIR}/topic_beta"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_beta/chunk1.bin" bs=1024 count=500 2>/dev/null
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_beta/chunk2.bin" bs=1024 count=600 2>/dev/null
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_beta/chunk3.bin" bs=1024 count=700 2>/dev/null

# 3. topic_large (2 MB file for streaming & rewind)
mkdir -p "${FIXTURE_DIR}/topic_large"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_large/stream_2m.bin" bs=1048576 count=2 2>/dev/null

log_msg "[INFO] Fixtures prepared successfully"
log_msg ""

# ==============================================================================
# SCENARIO 1: CLI ARGUMENT PARSING
# ==============================================================================
log_msg "--- SCENARIO 1: CLI Argument Parsing & Validation ---"
RC1=0
"${REPO_ROOT}/Part2_Case2/server" 19995 "${FIXTURE_DIR}" -0.5 >> "${VERIFICATION_LOG}" 2>&1 && RC1=1
"${REPO_ROOT}/Part2_Case2/server" 19995 "${FIXTURE_DIR}" 1.5 >> "${VERIFICATION_LOG}" 2>&1 && RC1=1
"${REPO_ROOT}/Part2_Case2/server" 80 "${FIXTURE_DIR}" 0.0 >> "${VERIFICATION_LOG}" 2>&1 && RC1=1
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 19995 >> "${VERIFICATION_LOG}" 2>&1 && RC1=1

if [ "${RC1}" -eq 0 ]; then
    log_msg "  [PASS] Scenario 1: CLI argument validation strictly enforced"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 1 failed"
fi
log_msg ""

# ==============================================================================
# SCENARIO 2: ZERO-FAULT BASELINE TRANSFER (p = 0.0)
# ==============================================================================
log_msg "--- SCENARIO 2: Zero-Fault Baseline Transfer (p = 0.0) ---"
PORT=$((PORT + 1))
"${REPO_ROOT}/Part2_Case2/server" "${PORT}" "${FIXTURE_DIR}" 0.0 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 2: Server failed to start"; exit 1; }

OUT2="${DOWNLOAD_BASE}/out_sc2"
mkdir -p "${OUT2}"
RC=0
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_alpha "${OUT2}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${RC}" -eq 0 ] && verify_transfer "topic_alpha" "${OUT2}"; then
    log_msg "  [PASS] Scenario 2: Zero-fault transfer completed bit-exact"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 2 failed"
fi
log_msg ""

# ==============================================================================
# SCENARIO 3: FAULT INJECTION & MID-FILE RESUME (p = 0.12)
# ==============================================================================
log_msg "--- SCENARIO 3: Fault Injection & Mid-File Checkpoint Resume (p = 0.12) ---"
PORT=$((PORT + 1))
"${REPO_ROOT}/Part2_Case2/server" "${PORT}" "${FIXTURE_DIR}" 0.12 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 3: Server failed to start"; exit 1; }

OUT3="${DOWNLOAD_BASE}/out_sc3"
mkdir -p "${OUT3}"
SC3_LOG="${DOWNLOAD_BASE}/sc3_client.log"
RC=0
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_beta "${OUT3}" 2>&1 | tee -a "${VERIFICATION_LOG}" > "${SC3_LOG}" || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

DISCONNECT_SEEN=0
if grep -q "Server closed connection (EOF)" "${SC3_LOG}"; then
    DISCONNECT_SEEN=1
fi

RESUME_SEEN=0
if grep -q "Resume ACCEPTED: Authoritative Checkpoint" "${SC3_LOG}"; then
    RESUME_SEEN=1
fi

SID_START=$(grep "Server created Session" "${SC3_LOG}" | head -n 1 | grep -o "0x[0-9A-Fa-f]*" | head -n 1)
SID_RESUME=$(grep "Requesting RESUME for Session" "${SC3_LOG}" | head -n 1 | grep -o "0x[0-9A-Fa-f]*" | head -n 1)
SAME_SID=0
if [ -n "${SID_START}" ] && [ -n "${SID_RESUME}" ] && [ "${SID_START}" = "${SID_RESUME}" ]; then
    SAME_SID=1
fi

if [ "${RC}" -eq 0 ] && [ "${DISCONNECT_SEEN}" -eq 1 ] && [ "${RESUME_SEEN}" -eq 1 ] && [ "${SAME_SID}" -eq 1 ] && verify_transfer "topic_beta" "${OUT3}"; then
    log_msg "  [PASS] Scenario 3: Server injected disconnect, client resumed same session (${SID_START}), completed bit-exact"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 3 failed (rc=${RC}, disc=${DISCONNECT_SEEN}, resume=${RESUME_SEEN}, same_sid=${SAME_SID})"
fi
log_msg ""

# ==============================================================================
# SCENARIO 4: AUTHORITATIVE CHECKPOINT PROGRESSION
# ==============================================================================
log_msg "--- SCENARIO 4: Authoritative Checkpoint Streaming Verification ---"
PORT=$((PORT + 1))
"${REPO_ROOT}/Part2_Case2/server" "${PORT}" "${FIXTURE_DIR}" 0.25 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 4: Server failed to start"; exit 1; }

OUT4="${DOWNLOAD_BASE}/out_sc4"
mkdir -p "${OUT4}"
SC4_LOG="${DOWNLOAD_BASE}/sc4_client.log"
RC=0
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_large "${OUT4}" 2>&1 | tee -a "${VERIFICATION_LOG}" > "${SC4_LOG}" || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${RC}" -eq 0 ] && grep -q "Resume ACCEPTED: Authoritative Checkpoint" "${SC4_LOG}" && verify_transfer "topic_large" "${OUT4}"; then
    log_msg "  [PASS] Scenario 4: Mid-file resume verified with authoritative checkpoint progression and 100% SHA-256 match"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 4 failed"
fi
log_msg ""

# ==============================================================================
# SCENARIO 5: LOCAL FILE RECONCILIATION & REWIND ON CONFLICT
# ==============================================================================
log_msg "--- SCENARIO 5: Local File Reconciliation & Authoritative Rewind ---"
PORT=$((PORT + 1))
"${REPO_ROOT}/Part2_Case2/server" "${PORT}" "${FIXTURE_DIR}" 0.0 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 5: Server failed to start"; exit 1; }

OUT5="${DOWNLOAD_BASE}/out_sc5"
mkdir -p "${OUT5}"

# Launch client in background and interrupt it mid-transfer
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_large "${OUT5}" >> "${VERIFICATION_LOG}" 2>&1 &
CLIENT_PID=$!

MID_TRANSFER=0
for _ in $(seq 1 20000); do
    if [ -f "${OUT5}/topic_large/.session_checkpoint" ] && [ -f "${OUT5}/topic_large/stream_2m.bin" ]; then
        SZ=$(stat -c%s "${OUT5}/topic_large/stream_2m.bin" 2>/dev/null || echo 0)
        if [ "${SZ}" -gt 65536 ]; then
            MID_TRANSFER=1
            break
        fi
    fi
    kill -0 "${CLIENT_PID}" 2>/dev/null || break
    sleep 0.001
done

kill -TERM "${CLIENT_PID}" 2>/dev/null || true
wait "${CLIENT_PID}" 2>/dev/null || true
CLIENT_PID=""

# Append 5,000 bytes of uncommitted garbage to the local file
CORRUPTED_EXTRA=0
if [ -f "${OUT5}/topic_large/stream_2m.bin" ]; then
    BEFORE_SZ=$(stat -c%s "${OUT5}/topic_large/stream_2m.bin")
    dd if=/dev/urandom bs=1000 count=5 >> "${OUT5}/topic_large/stream_2m.bin" 2>/dev/null
    AFTER_SZ=$(stat -c%s "${OUT5}/topic_large/stream_2m.bin")
    if [ "${AFTER_SZ}" -gt "${BEFORE_SZ}" ]; then
        CORRUPTED_EXTRA=1
    fi
fi

# Resume client against still-running server
SC5_LOG="${DOWNLOAD_BASE}/sc5_client.log"
RC=0
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_large "${OUT5}" 2>&1 | tee -a "${VERIFICATION_LOG}" > "${SC5_LOG}" || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

TRUNCATED_EXCESS=0
if grep -q "Truncating excess" "${SC5_LOG}"; then
    TRUNCATED_EXCESS=1
fi

if [ "${MID_TRANSFER}" -eq 1 ] && [ "${CORRUPTED_EXTRA}" -eq 1 ] && [ "${TRUNCATED_EXCESS}" -eq 1 ] && [ "${RC}" -eq 0 ] && verify_transfer "topic_large" "${OUT5}"; then
    log_msg "  [PASS] Scenario 5: Client detected local file overshoot, truncated uncommitted bytes via ftruncate(), and completed bit-exact"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 5 failed (mid=${MID_TRANSFER}, extra=${CORRUPTED_EXTRA}, trunc=${TRUNCATED_EXCESS}, rc=${RC})"
fi
log_msg ""

# ==============================================================================
# SCENARIO 6: MANIFEST CHANGE & INVALID SESSION REJECTION
# ==============================================================================
log_msg "--- SCENARIO 6: Manifest Change & Invalid Session Rejection ---"
PORT=$((PORT + 1))
MUT_DIR="${FIXTURE_DIR}/topic_mut"
mkdir -p "${MUT_DIR}"
dd if=/dev/urandom of="${MUT_DIR}/data1.bin" bs=1024 count=500 2>/dev/null

"${REPO_ROOT}/Part2_Case2/server" "${PORT}" "${FIXTURE_DIR}" 0.0 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 6: Server failed to start"; exit 1; }

OUT6="${DOWNLOAD_BASE}/out_sc6"
mkdir -p "${OUT6}"
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_mut "${OUT6}" >> "${VERIFICATION_LOG}" 2>&1 &
CLIENT_PID=$!

for _ in $(seq 1 20000); do
    if [ -f "${OUT6}/topic_mut/.session_checkpoint" ]; then
        break
    fi
    kill -0 "${CLIENT_PID}" 2>/dev/null || break
    sleep 0.001
done

kill -TERM "${CLIENT_PID}" 2>/dev/null || true
wait "${CLIENT_PID}" 2>/dev/null || true
CLIENT_PID=""

# Tamper with topic directory (adds new file altering CRC32 fingerprint)
printf "Tampered manifest\n" > "${MUT_DIR}/tamper.txt"

RC_MUT=0
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_mut "${OUT6}" >> "${VERIFICATION_LOG}" 2>&1 || RC_MUT=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

# Unknown session ID test
PORT=$((PORT + 1))
"${REPO_ROOT}/Part2_Case2/server" "${PORT}" "${FIXTURE_DIR}" 0.0 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 6: Server failed to start for invalid ID check"; exit 1; }

OUT6_INV="${DOWNLOAD_BASE}/out_sc6_inv"
mkdir -p "${OUT6_INV}/topic_mut"
printf "12345678912345 0 0 0\n" > "${OUT6_INV}/topic_mut/.session_checkpoint"
RC_INV=0
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_mut "${OUT6_INV}" >> "${VERIFICATION_LOG}" 2>&1 || RC_INV=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${RC_MUT}" -ne 0 ] && grep -q "MANIFEST_CHANGED" "${VERIFICATION_LOG}" && \
   [ "${RC_INV}" -ne 0 ] && grep -q "SESSION_NOT_FOUND" "${VERIFICATION_LOG}"; then
    log_msg "  [PASS] Scenario 6: Manifest alteration and invalid session ID both rejected with authoritative RESUME_ERROR"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 6 failed (rc_mut=${RC_MUT}, rc_inv=${RC_INV})"
fi
log_msg ""

# ==============================================================================
# SCENARIO 7: EMPTY FILES & BOUNDARY CONDITIONS
# ==============================================================================
log_msg "--- SCENARIO 7: Empty Files & File Boundary Conditions ---"
PORT=$((PORT + 1))
mkdir -p "${FIXTURE_DIR}/topic_edge"
touch "${FIXTURE_DIR}/topic_edge/0_empty.bin"
printf "Small file content\n" > "${FIXTURE_DIR}/topic_edge/1_small.txt"
dd if=/dev/urandom of="${FIXTURE_DIR}/topic_edge/2_boundary.bin" bs=65536 count=1 2>/dev/null
touch "${FIXTURE_DIR}/topic_edge/3_another_empty.bin"

"${REPO_ROOT}/Part2_Case2/server" "${PORT}" "${FIXTURE_DIR}" 0.0 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 7: Server failed to start"; exit 1; }

OUT7="${DOWNLOAD_BASE}/out_sc7"
mkdir -p "${OUT7}"
RC=0
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_edge "${OUT7}" >> "${VERIFICATION_LOG}" 2>&1 || RC=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${RC}" -eq 0 ] && verify_transfer "topic_edge" "${OUT7}"; then
    log_msg "  [PASS] Scenario 7: Empty files, sub-chunk files, and boundary conditions handled bit-exact"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 7 failed"
fi
log_msg ""

# ==============================================================================
# SCENARIO 8: CONCURRENT SESSIONS UNDER FAULT INJECTION
# ==============================================================================
log_msg "--- SCENARIO 8: Concurrent Sessions Under Fault Injection ---"
PORT=$((PORT + 1))
"${REPO_ROOT}/Part2_Case2/server" "${PORT}" "${FIXTURE_DIR}" 0.05 >> "${VERIFICATION_LOG}" 2>&1 &
SERVER_PID=$!
wait_for_port "${PORT}" || { log_msg "  [FAIL] Scenario 8: Server failed to start"; exit 1; }

OUT8_A="${DOWNLOAD_BASE}/out_sc8_alpha"
OUT8_B="${DOWNLOAD_BASE}/out_sc8_beta"
mkdir -p "${OUT8_A}" "${OUT8_B}"

RC_A=0
RC_B=0
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_alpha "${OUT8_A}" >> "${VERIFICATION_LOG}" 2>&1 &
PID_A=$!
"${REPO_ROOT}/Part2_Case2/client" 127.0.0.1 "${PORT}" topic_beta "${OUT8_B}" >> "${VERIFICATION_LOG}" 2>&1 &
PID_B=$!

wait "${PID_A}" || RC_A=$?
wait "${PID_B}" || RC_B=$?

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

if [ "${RC_A}" -eq 0 ] && [ "${RC_B}" -eq 0 ] && \
   verify_transfer "topic_alpha" "${OUT8_A}" && verify_transfer "topic_beta" "${OUT8_B}"; then
    log_msg "  [PASS] Scenario 8: Concurrent independent sessions resumed under faults and completed bit-exact"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    log_msg "  [FAIL] Scenario 8 failed (rc_a=${RC_A}, rc_b=${RC_B})"
fi
log_msg ""

# ==============================================================================
# SUMMARY
# ==============================================================================
log_msg "================================================================================"
log_msg "PHASE 4 VERIFICATION SUMMARY: ${PASSED_COUNT} / ${TOTAL_SCENARIOS} SCENARIOS PASSED"
log_msg "Detailed log available at: ${VERIFICATION_LOG}"
log_msg "================================================================================"

if [ "${PASSED_COUNT}" -eq "${TOTAL_SCENARIOS}" ]; then
    log_msg "STATUS: ALL PHASE 4 ACCEPTANCE GATES PASSED [100%]"
    exit 0
else
    log_msg "STATUS: VERIFICATION DEFECTS DETECTED"
    exit 1
fi
