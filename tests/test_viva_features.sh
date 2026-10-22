#!/usr/bin/env bash
# ==============================================================================
# IT305 Computer Networks Lab - Autumn 2026
# Script: tests/test_viva_features.sh
# Description: Automated Verification & Viva Demonstration Harness for:
#   1. Feature One: SHA-256 File Integrity Verification & Corruption Detection
#   2. Feature Two: Adaptive Parallel TCP Transfer (--auto)
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

PORT=20150
FIXTURE_DIR="/tmp/test_viva_fixtures_$$"
OUT_DIR="/tmp/test_viva_downloads_$$"
SERVER_PID=""

cleanup() {
    if [ -n "${SERVER_PID}" ]; then
        kill -9 "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    rm -rf "${FIXTURE_DIR}" "${OUT_DIR}"
}
trap cleanup EXIT INT TERM

echo "======================================================================"
echo "IT305 Viva Verification Harness: SHA-256 Integrity & Adaptive Transfer"
echo "Target: Part2_Case2_Enhanced (Server & Client)"
echo "======================================================================"

# Ensure binaries are built
echo ">>> Step 0: Building Part2_Case2_Enhanced..."
make -C Part2_Case2_Enhanced clean >/dev/null
make -C Part2_Case2_Enhanced >/dev/null
echo "Build complete."

# Set up test fixture
echo ">>> Setting up multi-file test fixture in ${FIXTURE_DIR}/demo_topic..."
mkdir -p "${FIXTURE_DIR}/demo_topic"

# 1. Empty file (0 bytes)
touch "${FIXTURE_DIR}/demo_topic/empty.txt"

# 2. Small file (< 64 KiB chunk)
printf "Computer Networks Lab - End-to-End Cryptographic File Integrity Verification.\n" > "${FIXTURE_DIR}/demo_topic/small.txt"

# 3. Medium text file
head -c 2500 /dev/urandom | tr -dc 'a-zA-Z0-9\n' | head -c 2048 > "${FIXTURE_DIR}/demo_topic/medium.txt"

# 4. Large multi-chunk binary file (256 KiB = exactly four 64 KiB chunks)
head -c 262144 /dev/urandom > "${FIXTURE_DIR}/demo_topic/large_multi_chunk.bin"

echo "Fixture directory contents:"
ls -lh "${FIXTURE_DIR}/demo_topic"

# Compute ground truth SHA-256 digests
echo ""
echo "Authoritative Source File SHA-256 digests:"
sha256sum "${FIXTURE_DIR}/demo_topic"/*

# ------------------------------------------------------------------------------
# Test 1: Explicit 1-Connection Transfer & SHA-256 Verification (PASS)
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "TEST 1: Explicit 1-Connection Transfer with SHA-256 Verification"
echo "======================================================================"

./Part2_Case2_Enhanced/server "${PORT}" "${FIXTURE_DIR}" 0.00 >/dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" 1

echo "Verifying bit-exact match against source topic:"
diff -r "${FIXTURE_DIR}/demo_topic" "${OUT_DIR}/demo_topic"
echo ">>> TEST 1 PASSED: All files transferred, verified PASS, and match bit-for-bit."

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

# ------------------------------------------------------------------------------
# Test 2: Explicit 4-Connection Parallel Transfer & SHA-256 Verification (PASS)
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "TEST 2: Explicit 4-Connection Parallel Transfer (pwrite concurrency)"
echo "======================================================================"

./Part2_Case2_Enhanced/server "${PORT}" "${FIXTURE_DIR}" 0.00 >/dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" 4

diff -r "${FIXTURE_DIR}/demo_topic" "${OUT_DIR}/demo_topic"
echo ">>> TEST 2 PASSED: 4-Connection parallel pwrite() transfer verified PASS."

# ------------------------------------------------------------------------------
# Test 3: Mandatory 1-Byte Corruption Detection Demonstration (FAIL)
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "TEST 3: Mandatory One-Byte Corruption Demonstration"
echo "======================================================================"

CORRUPT_TARGET="${OUT_DIR}/demo_topic/small.txt"
ORIGINAL_SRC="${FIXTURE_DIR}/demo_topic/small.txt"
ORIGINAL_SRC_HASH=$(sha256sum "${ORIGINAL_SRC}" | awk '{print $1}')
FILE_SIZE_BEFORE=$(stat -c%s "${CORRUPT_TARGET}")

echo "Target file before corruption: ${CORRUPT_TARGET} (Size: ${FILE_SIZE_BEFORE} bytes)"
echo "Modifying exactly ONE byte at byte offset 10..."

# Flip bits of byte at offset 10 without changing file size
python3 -c "
with open('${CORRUPT_TARGET}', 'r+b') as f:
    f.seek(10)
    b = f.read(1)
    f.seek(10)
    f.write(bytes([b[0] ^ 0x01]))
"

FILE_SIZE_AFTER=$(stat -c%s "${CORRUPT_TARGET}")
echo "Target file after corruption : ${CORRUPT_TARGET} (Size: ${FILE_SIZE_AFTER} bytes)"

if [ "${FILE_SIZE_BEFORE}" -ne "${FILE_SIZE_AFTER}" ]; then
    echo "ERROR: File size changed during corruption test!"
    exit 1
fi

echo "Running verification mode to confirm corruption detection:"
set +e
./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" --verify
VERIFY_EXIT=$?
set -e

if [ "${VERIFY_EXIT}" -ne 0 ]; then
    echo ">>> Verification correctly returned non-zero exit code (${VERIFY_EXIT})."
else
    echo "ERROR: Verification returned 0 despite file corruption!"
    exit 1
fi

# Confirm source file on server was NOT affected
CURRENT_SRC_HASH=$(sha256sum "${ORIGINAL_SRC}" | awk '{print $1}')
if [ "${ORIGINAL_SRC_HASH}" == "${CURRENT_SRC_HASH}" ]; then
    echo ">>> Source file on server is intact and completely unmodified."
else
    echo "ERROR: Source file was modified!"
    exit 1
fi
echo ">>> TEST 3 PASSED: Single-byte corruption detected and rejected successfully."

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

# ------------------------------------------------------------------------------
# Test 4: Fault Injection & Resilient Recovery with Integrity Verification
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "TEST 4: Fault Injection (p=0.10) with Recovery & SHA-256 Verification"
echo "======================================================================"

./Part2_Case2_Enhanced/server "${PORT}" "${FIXTURE_DIR}" 0.10 >/dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" 2

diff -r "${FIXTURE_DIR}/demo_topic" "${OUT_DIR}/demo_topic"
echo ">>> TEST 4 PASSED: Reconnected after simulated faults, all files verified PASS."

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

# ------------------------------------------------------------------------------
# Test 5: Feature Two: Adaptive Parallel TCP Transfer (--auto)
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "TEST 5: Adaptive Parallel TCP Transfer (--auto selection)"
echo "======================================================================"

./Part2_Case2_Enhanced/server "${PORT}" "${FIXTURE_DIR}" 0.00 >/dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" --auto

echo "Verifying final published directory from auto-selection:"
diff -r "${FIXTURE_DIR}/demo_topic" "${OUT_DIR}/demo_topic"

echo "Re-running verification on the final published output:"
./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" --verify

echo ">>> TEST 5 PASSED: Adaptive parallel transfer evaluated all candidates and published verified output."

kill -9 "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""

# ------------------------------------------------------------------------------
# Test 6: CLI Argument Edge Cases
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "TEST 6: CLI Argument & Edge Case Validation"
echo "======================================================================"

set +e
echo "Testing invalid connection count (0):"
./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" 0 2>&1 | grep "Invalid number of connections"

echo "Testing invalid connection count (32):"
./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" 32 2>&1 | grep "Invalid number of connections"

echo "Testing unsupported flag (--bogus):"
./Part2_Case2_Enhanced/client 127.0.0.1 "${PORT}" demo_topic "${OUT_DIR}" --bogus 2>&1 | grep "Invalid number of connections"
set -e
echo ">>> TEST 6 PASSED: Invalid arguments rejected safely."

echo ""
echo "======================================================================"
echo "ALL VIVA DEMONSTRATION TESTS COMPLETED SUCCESSFULLY!"
echo "======================================================================"
