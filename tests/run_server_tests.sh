#!/usr/bin/env bash
# ==============================================================================
# IT305 Computer Networks Lab - Autumn 2026
# Project: Fault-Tolerant Topic-Based File Distribution Service
#
# Script: tests/run_server_tests.sh
# Description: Automated Integration Test Harness for Part1/server.c.
#
# Author: Jay Vaghela (Student ID: 202401237)
# Phase: Phase 1 (Task 1.3 Server Verification)
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PORT=19998
FIXTURE_DIR="/tmp/test_topics_harness_$$"

cleanup() {
    if [ -n "${SERVER_PID:-}" ]; then
        kill -TERM "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    rm -rf "${FIXTURE_DIR}"
    rm -f "${REPO_ROOT}/tests/test_server_phase1"
}
trap cleanup EXIT

if [ ! -f "${REPO_ROOT}/Part1/server" ]; then
    echo ">>> Building Part1/server..."
    (cd "${REPO_ROOT}/Part1" && make clean && make)
fi

echo ">>> Compiling test_server_phase1 test client..."
gcc -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I"${REPO_ROOT}/common" \
    "${REPO_ROOT}/common/protocol.c" "${REPO_ROOT}/tests/test_server_phase1.c" \
    -o "${REPO_ROOT}/tests/test_server_phase1"

echo ">>> Setting up temporary test fixtures in ${FIXTURE_DIR}..."
rm -rf "${FIXTURE_DIR}"
mkdir -p "${FIXTURE_DIR}/valid_topic"

# Create test files with distinct contents
printf '1234567890123456789012345' > "${FIXTURE_DIR}/valid_topic/file_a.txt" # 25 bytes
head -c 100 /dev/zero | tr '\0' 'B' > "${FIXTURE_DIR}/valid_topic/file_b.txt" # 100 bytes
printf '1234567890' > "${FIXTURE_DIR}/valid_topic/file_c.txt"                 # 10 bytes

echo ">>> 1. Starting Part1/server in MULTI-THREADED (MT) mode on port ${PORT}..."
"${REPO_ROOT}/Part1/server" "${PORT}" "${FIXTURE_DIR}" mt &
SERVER_PID=$!
sleep 0.5

echo ">>> 2. Executing server integration test suite against MT server..."
"${REPO_ROOT}/tests/test_server_phase1" "${PORT}"

echo ">>> 3. Stopping MT server..."
kill -TERM "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

echo ""
echo ">>> 4. Starting Part1/server in SEQUENTIAL mode on port ${PORT}..."
"${REPO_ROOT}/Part1/server" "${PORT}" "${FIXTURE_DIR}" sequential &
SERVER_PID=$!
sleep 0.5

echo ">>> 5. Executing server integration test suite against SEQUENTIAL server..."
# For sequential server, test valid transfer, invalid topic, traversal, disconnect
"${REPO_ROOT}/tests/test_server_phase1" "${PORT}"

echo ">>> 6. Stopping SEQUENTIAL server..."
kill -TERM "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

echo ""
echo "==============================================================="
echo "ALL SERVER INTEGRATION TESTS PASSED SUCCESSFULLY (MT & SEQUENTIAL)"
echo "==============================================================="
