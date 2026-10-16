#!/usr/bin/env bash
# ==============================================================================
# IT305 Computer Networks Lab - Autumn 2026
# Project: Fault-Tolerant Topic-Based File Distribution Service
#
# Script: scripts/test_correctness.sh
# Description: Automated Multi-Case Regression & Correctness Test Harness
# Phase: Phase 6 (Sole Owner: Yug Malaviya - 202401256)
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LOG_FILE="${ROOT_DIR}/tests/full_regression.log"

mkdir -p "${ROOT_DIR}/tests"
: > "${LOG_FILE}"

PASS_COUNT=0
FAIL_COUNT=0

log() {
    echo "$1" | tee -a "${LOG_FILE}"
}

record_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    log "[PASS] $1"
}

record_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    log "[FAIL] $1"
}

TEST_SANDBOX="/tmp/it305_regression_$$"
DATASET_DIR="${TEST_SANDBOX}/dataset"
OUT_DIR="${TEST_SANDBOX}/downloads"

cleanup() {
    pkill -9 -f "${ROOT_DIR}/Part1/server" 2>/dev/null || true
    pkill -9 -f "${ROOT_DIR}/Part2_Case1/server" 2>/dev/null || true
    pkill -9 -f "${ROOT_DIR}/Part2_Case2/server" 2>/dev/null || true
    pkill -9 -f "${ROOT_DIR}/Part2_Case2_Enhanced/server" 2>/dev/null || true
    rm -rf "${TEST_SANDBOX}"
}
trap cleanup EXIT

log "======================================================================"
log "IT305 Master 4-Variant Correctness & Regression Test Harness (Phase 6)"
log "Timestamp: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
log "======================================================================"

# ------------------------------------------------------------------------------
# 1. Build Validation Across All 4 Folders & Logging Unit Test
# ------------------------------------------------------------------------------
log ""
log "--- Section 1: Strict Compiler Build Validation (-Wall -Wextra -Werror) ---"

for dir in Part1 Part2_Case1 Part2_Case2 Part2_Case2_Enhanced; do
    if make -C "${ROOT_DIR}/${dir}" clean >>"${LOG_FILE}" 2>&1 && \
       make -C "${ROOT_DIR}/${dir}" >>"${LOG_FILE}" 2>&1; then
        if [[ -x "${ROOT_DIR}/${dir}/server" && -x "${ROOT_DIR}/${dir}/client" ]]; then
            record_pass "Clean build & binaries verified in ${dir}/"
        else
            record_fail "Missing server or client binary in ${dir}/"
        fi
    else
        record_fail "Build failed or produced warnings in ${dir}/"
    fi
done

mkdir -p "${TEST_SANDBOX}"
cat << 'CEOF' > "${TEST_SANDBOX}/test_logging.c"
#define _POSIX_C_SOURCE 200809L
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int main(void) {
    uint64_t t1 = get_monotonic_time_ns();
    struct timespec ts = {0, 1000000L};
    nanosleep(&ts, NULL);
    uint64_t t2 = get_monotonic_time_ns();
    if (t1 == 0 || t2 <= t1) return 1;

    log_event(LOG_INFO, "UNIT_TEST", "Monotonic delta = %lu ns", (unsigned long)(t2 - t1));

    BenchmarkRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.run_id = 1;
    strncpy(rec.case_name, "UnitTest", sizeof(rec.case_name) - 1);
    strncpy(rec.topic, "cane", sizeof(rec.topic) - 1);
    rec.num_clients = 1;
    rec.num_connections = 1;
    rec.failure_prob = 0.05;
    rec.useful_bytes = 1048576;
    rec.redundant_bytes = 65536;
    rec.total_socket_bytes = 1114368;
    rec.start_monotonic_ns = t1;
    rec.end_monotonic_ns = t2;

    const char *csv_file = "/tmp/test_logging_unit.csv";
    unlink(csv_file);
    if (record_benchmark_metric(&rec, csv_file) != 0) return 2;
    unlink(csv_file);
    return 0;
}
CEOF

if gcc -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I"${ROOT_DIR}/common" \
    "${TEST_SANDBOX}/test_logging.c" "${ROOT_DIR}/common/logging.c" \
    -o "${TEST_SANDBOX}/test_logging" >>"${LOG_FILE}" 2>&1 && \
   "${TEST_SANDBOX}/test_logging" >>"${LOG_FILE}" 2>&1; then
    record_pass "Phase 6 common/logging.* unit test (monotonic clock + thread-safe CSV)"
else
    record_fail "Phase 6 common/logging.* unit test failed"
fi

# ------------------------------------------------------------------------------
# 2. Fast Deterministic Binary Dataset Generation
# ------------------------------------------------------------------------------
log ""
log "--- Section 2: Preparing Synthetic & Binary Topic Datasets ---"
python3 - "${DATASET_DIR}" << 'PYEOF'
import os, sys
from pathlib import Path

base = Path(sys.argv[1])
for t in ["empty_topic", "small_topic", "large_topic", "cane", "gatto", "cavallo", "elefante"]:
    (base / t).mkdir(parents=True, exist_ok=True)

# 100-byte small file
(base / "small_topic" / "tiny_100b.bin").write_bytes(os.urandom(100))

# 2 MB single large binary file
(base / "large_topic" / "large_2mb.bin").write_bytes(os.urandom(2 * 1024 * 1024))

# Multi-file binary topics (~750 KB per topic across 5 files)
for i in range(1, 6):
    (base / "cane" / f"cane_img_0{i}.jpg").write_bytes(os.urandom(100000 + i * 16384))
    (base / "gatto" / f"gatto_img_0{i}.jpg").write_bytes(os.urandom(95000 + i * 16384))
    (base / "cavallo" / f"cavallo_img_0{i}.jpg").write_bytes(os.urandom(98000 + i * 16384))
    (base / "elefante" / f"elefante_img_0{i}.jpg").write_bytes(os.urandom(92000 + i * 16384))
PYEOF
record_pass "Dataset created (empty_topic, small_topic, large_topic, cane, gatto, cavallo, elefante)"

# ------------------------------------------------------------------------------
# 3. CLI Argument Validation Tests
# ------------------------------------------------------------------------------
log ""
log "--- Section 3: CLI Argument & Boundary Validation ---"

if ! "${ROOT_DIR}/Part1/server" >>"${LOG_FILE}" 2>&1; then
    record_pass "Part1/server rejects missing CLI arguments"
else
    record_fail "Part1/server accepted missing CLI arguments"
fi

if ! "${ROOT_DIR}/Part1/server" 70000 "${DATASET_DIR}" >>"${LOG_FILE}" 2>&1; then
    record_pass "Part1/server rejects invalid port (70000)"
else
    record_fail "Part1/server accepted invalid port (70000)"
fi

for dir in Part2_Case1 Part2_Case2 Part2_Case2_Enhanced; do
    if ! "${ROOT_DIR}/${dir}/server" 19100 "${DATASET_DIR}" -0.1 >>"${LOG_FILE}" 2>&1 && \
       ! "${ROOT_DIR}/${dir}/server" 19100 "${DATASET_DIR}" 1.5 >>"${LOG_FILE}" 2>&1; then
        record_pass "${dir}/server rejects out-of-range failure probabilities (-0.1, 1.5)"
    else
        record_fail "${dir}/server accepted invalid failure probability"
    fi
done

if ! "${ROOT_DIR}/Part2_Case2_Enhanced/client" 127.0.0.1 19100 cane "${OUT_DIR}" 0 >>"${LOG_FILE}" 2>&1 && \
   ! "${ROOT_DIR}/Part2_Case2_Enhanced/client" 127.0.0.1 19100 cane "${OUT_DIR}" 32 >>"${LOG_FILE}" 2>&1; then
    record_pass "Part2_Case2_Enhanced/client rejects invalid connection counts (0, 32)"
else
    record_fail "Part2_Case2_Enhanced/client accepted invalid connection counts"
fi

# ------------------------------------------------------------------------------
# 4. Part 1 End-to-End, Edge Topics, Path Traversal & Multi-Client Concurrency
# ------------------------------------------------------------------------------
log ""
log "--- Section 4: Part 1 Baseline (Edge Topics, Security & Concurrency) ---"

P1_PORT=19201
"${ROOT_DIR}/Part1/server" ${P1_PORT} "${DATASET_DIR}" >>"${LOG_FILE}" 2>&1 &
P1_PID=$!
sleep 0.15

if ! "${ROOT_DIR}/Part1/client" 127.0.0.1 ${P1_PORT} "../etc" "${OUT_DIR}/p1_sec" >>"${LOG_FILE}" 2>&1; then
    record_pass "Part 1 rejects path traversal topic ('../etc')"
else
    record_fail "Part 1 allowed path traversal topic ('../etc')"
fi

for topic in empty_topic small_topic large_topic cane; do
    mkdir -p "${OUT_DIR}/p1_${topic}"
    if "${ROOT_DIR}/Part1/client" 127.0.0.1 ${P1_PORT} "${topic}" "${OUT_DIR}/p1_${topic}" >>"${LOG_FILE}" 2>&1 && \
       diff -r "${DATASET_DIR}/${topic}" "${OUT_DIR}/p1_${topic}/${topic}" >>"${LOG_FILE}" 2>&1; then
        record_pass "Part 1 bit-exact transfer for topic '${topic}'"
    else
        record_fail "Part 1 transfer or diff failed for topic '${topic}'"
    fi
done

mkdir -p "${OUT_DIR}/p1_conc"
"${ROOT_DIR}/Part1/client" 127.0.0.1 ${P1_PORT} cane "${OUT_DIR}/p1_conc/c1" >>"${LOG_FILE}" 2>&1 &
C1=$!
"${ROOT_DIR}/Part1/client" 127.0.0.1 ${P1_PORT} gatto "${OUT_DIR}/p1_conc/c2" >>"${LOG_FILE}" 2>&1 &
C2=$!
"${ROOT_DIR}/Part1/client" 127.0.0.1 ${P1_PORT} cavallo "${OUT_DIR}/p1_conc/c3" >>"${LOG_FILE}" 2>&1 &
C3=$!
"${ROOT_DIR}/Part1/client" 127.0.0.1 ${P1_PORT} elefante "${OUT_DIR}/p1_conc/c4" >>"${LOG_FILE}" 2>&1 &
C4=$!

if wait $C1 && wait $C2 && wait $C3 && wait $C4 && \
   diff -r "${DATASET_DIR}/cane" "${OUT_DIR}/p1_conc/c1/cane" >>"${LOG_FILE}" 2>&1 && \
   diff -r "${DATASET_DIR}/gatto" "${OUT_DIR}/p1_conc/c2/gatto" >>"${LOG_FILE}" 2>&1 && \
   diff -r "${DATASET_DIR}/cavallo" "${OUT_DIR}/p1_conc/c3/cavallo" >>"${LOG_FILE}" 2>&1 && \
   diff -r "${DATASET_DIR}/elefante" "${OUT_DIR}/p1_conc/c4/elefante" >>"${LOG_FILE}" 2>&1; then
    record_pass "Part 1 concurrent 4-client transfer verified (100% diff match)"
else
    record_fail "Part 1 concurrent 4-client transfer failed"
fi

kill -9 ${P1_PID} 2>/dev/null || true
wait ${P1_PID} 2>/dev/null || true

# ------------------------------------------------------------------------------
# 5. Part 2 Case 1: Fault Injection & Full Topic Restart
# ------------------------------------------------------------------------------
log ""
log "--- Section 5: Part 2 Case 1 (Fault Injection & Full Restart) ---"

C1_PORT=19202
"${ROOT_DIR}/Part2_Case1/server" ${C1_PORT} "${DATASET_DIR}" 0.05 >>"${LOG_FILE}" 2>&1 &
C1_PID=$!
sleep 0.15

mkdir -p "${OUT_DIR}/c1_out"
if "${ROOT_DIR}/Part2_Case1/client" 127.0.0.1 ${C1_PORT} cane "${OUT_DIR}/c1_out" >>"${LOG_FILE}" 2>&1 && \
   diff -r "${DATASET_DIR}/cane" "${OUT_DIR}/c1_out/cane" >>"${LOG_FILE}" 2>&1; then
    record_pass "Part 2 Case 1 fault recovery (p=0.05) & bit-exact diff verified"
else
    record_fail "Part 2 Case 1 fault recovery failed"
fi

kill -9 ${C1_PID} 2>/dev/null || true
wait ${C1_PID} 2>/dev/null || true

# ------------------------------------------------------------------------------
# 6. Part 2 Case 2: Session Management & Mid-File Checkpoint Resume
# ------------------------------------------------------------------------------
log ""
log "--- Section 6: Part 2 Case 2 (Session Management & Checkpoint Resume) ---"

C2_PORT=19203
"${ROOT_DIR}/Part2_Case2/server" ${C2_PORT} "${DATASET_DIR}" 0.10 >>"${LOG_FILE}" 2>&1 &
C2_PID=$!
sleep 0.15

mkdir -p "${OUT_DIR}/c2_out"
if "${ROOT_DIR}/Part2_Case2/client" 127.0.0.1 ${C2_PORT} cane "${OUT_DIR}/c2_out" >>"${LOG_FILE}" 2>&1 && \
   diff -r "${DATASET_DIR}/cane" "${OUT_DIR}/c2_out/cane" >>"${LOG_FILE}" 2>&1; then
    record_pass "Part 2 Case 2 mid-file checkpoint resume (p=0.10) & bit-exact diff verified"
else
    record_fail "Part 2 Case 2 mid-file checkpoint resume failed"
fi

kill -9 ${C2_PID} 2>/dev/null || true
wait ${C2_PID} 2>/dev/null || true

# ------------------------------------------------------------------------------
# 7. Part 2 Case 2 Enhanced: Default 4-Arg CLI & 4-Connection Parallel pwrite()
# ------------------------------------------------------------------------------
log ""
log "--- Section 7: Part 2 Case 2 Enhanced (4-Arg Fallback & Parallel TCP) ---"

ENH_PORT1=19204
"${ROOT_DIR}/Part2_Case2_Enhanced/server" ${ENH_PORT1} "${DATASET_DIR}" 0.00 >>"${LOG_FILE}" 2>&1 &
ENH_PID1=$!
sleep 0.15

mkdir -p "${OUT_DIR}/enh_default"
if "${ROOT_DIR}/Part2_Case2_Enhanced/client" 127.0.0.1 ${ENH_PORT1} cane "${OUT_DIR}/enh_default" >>"${LOG_FILE}" 2>&1 && \
   diff -r "${DATASET_DIR}/cane" "${OUT_DIR}/enh_default/cane" >>"${LOG_FILE}" 2>&1; then
    record_pass "Part 2 Case 2 Enhanced mandatory 4-arg default CLI (1 connection) verified"
else
    record_fail "Part 2 Case 2 Enhanced mandatory 4-arg default CLI failed"
fi

kill -9 ${ENH_PID1} 2>/dev/null || true
wait ${ENH_PID1} 2>/dev/null || true

ENH_PORT2=19205
"${ROOT_DIR}/Part2_Case2_Enhanced/server" ${ENH_PORT2} "${DATASET_DIR}" 0.05 >>"${LOG_FILE}" 2>&1 &
ENH_PID2=$!
sleep 0.15

mkdir -p "${OUT_DIR}/enh_parallel"
if "${ROOT_DIR}/Part2_Case2_Enhanced/client" 127.0.0.1 ${ENH_PORT2} large_topic "${OUT_DIR}/enh_parallel" 4 >>"${LOG_FILE}" 2>&1 && \
   diff -r "${DATASET_DIR}/large_topic" "${OUT_DIR}/enh_parallel/large_topic" >>"${LOG_FILE}" 2>&1; then
    record_pass "Part 2 Case 2 Enhanced 4-connection parallel pwrite() under fault injection (p=0.05) verified"
else
    record_fail "Part 2 Case 2 Enhanced 4-connection parallel transfer failed"
fi

kill -9 ${ENH_PID2} 2>/dev/null || true
wait ${ENH_PID2} 2>/dev/null || true

# ------------------------------------------------------------------------------
# Summary Report
# ------------------------------------------------------------------------------
log ""
log "======================================================================"
log "REGRESSION SUMMARY: ${PASS_COUNT} PASSED, ${FAIL_COUNT} FAILED"
log "Full Log Saved To : ${LOG_FILE}"
log "======================================================================"

if [[ ${FAIL_COUNT} -ne 0 ]]; then
    exit 1
fi
exit 0
