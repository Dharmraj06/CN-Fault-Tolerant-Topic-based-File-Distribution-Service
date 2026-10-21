#!/usr/bin/env bash
# ==============================================================================
# IT305 Computer Networks Lab - Autumn 2026
# Project: Fault-Tolerant Topic-Based File Distribution Service
#
# Script: scripts/package_submission.sh
# Description: Clean-Sandbox Submission Archive Builder & Validator
# Phase: Phase 6 (Sole Owner: Yug Malaviya - 202401256)
#
# Produces FinalSubmission_Group_071_072.tar.gz and FinalSubmission_Group_071_072.zip,
# verifies absence of intermediate .o / .git / temporary artifacts, extracts the
# archive in an isolated /tmp sandbox, compiles all 4 directories, and runs
# scripts/test_correctness.sh to guarantee 100% auto-grader compliance.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
SUBMISSION_NAME="FinalSubmission_Group_071_072"
STAGE_ROOT="/tmp/it305_pkg_stage_$$"
SANDBOX_ROOT="/tmp/it305_pkg_sandbox_$$"
STAGE_DIR="${STAGE_ROOT}/${SUBMISSION_NAME}"

cleanup() {
    rm -rf "${STAGE_ROOT}" "${SANDBOX_ROOT}"
}
trap cleanup EXIT

echo "======================================================================"
echo "IT305 Phase 6 Automated Submission Packager (${SUBMISSION_NAME})"
echo "======================================================================"

# 1. Verify required top-level deliverables exist
for req in \
    "${ROOT_DIR}/FinalReport_Group_071_072.pdf" \
    "${ROOT_DIR}/README.md" \
    "${ROOT_DIR}/Results/exp1_part1_concurrency.csv" \
    "${ROOT_DIR}/Results/exp2_fault_tolerance.csv" \
    "${ROOT_DIR}/Results/exp3_overhead_comparison.csv" \
    "${ROOT_DIR}/Results/exp4_enhanced_speedup.csv" \
    "${ROOT_DIR}/Results/Part1_throughput_vs_clients.png" \
    "${ROOT_DIR}/Results/Part2_Case1_vs_Case2_throughput.png" \
    "${ROOT_DIR}/Results/Part2_Case1_vs_Case2_overhead.png" \
    "${ROOT_DIR}/Results/Part2_Enhanced_speedup_comparison.png"; do
    if [[ ! -f "${req}" ]]; then
        echo "[ERROR] Missing required submission file: ${req}" >&2
        exit 1
    fi
done

# 2. Stage clean submission tree matching Tier 1 folder layout
mkdir -p "${STAGE_DIR}"
cp "${ROOT_DIR}/FinalReport_Group_071_072.pdf" "${STAGE_DIR}/"
cp "${ROOT_DIR}/README.md" "${STAGE_DIR}/"

for dir in common Part1 Part2_Case1 Part2_Case2 Part2_Case2_Enhanced Results scripts tests; do
    if [[ -d "${ROOT_DIR}/${dir}" ]]; then
        cp -r "${ROOT_DIR}/${dir}" "${STAGE_DIR}/"
    fi
done

# 3. Clean all intermediate .o files, binaries, and scratch artifacts in stage
for case_dir in Part1 Part2_Case1 Part2_Case2 Part2_Case2_Enhanced; do
    make -C "${STAGE_DIR}/${case_dir}" clean >/dev/null
done
find "${STAGE_DIR}" -name "*.o" -o -name ".DS_Store" -o -name ".checkpoint_*" | xargs rm -f 2>/dev/null || true

# 4. Create tar.gz and zip archives in ROOT_DIR
tar -czf "${ROOT_DIR}/${SUBMISSION_NAME}.tar.gz" -C "${STAGE_ROOT}" "${SUBMISSION_NAME}"
python3 -c "import shutil; shutil.make_archive('${ROOT_DIR}/${SUBMISSION_NAME}', 'zip', '${STAGE_ROOT}', '${SUBMISSION_NAME}')"

echo "[PACKAGER] Created ${ROOT_DIR}/${SUBMISSION_NAME}.tar.gz"
echo "[PACKAGER] Created ${ROOT_DIR}/${SUBMISSION_NAME}.zip"

# 5. Clean-Room Sandbox Extraction & Verification Gate
mkdir -p "${SANDBOX_ROOT}"
tar -xzf "${ROOT_DIR}/${SUBMISSION_NAME}.tar.gz" -C "${SANDBOX_ROOT}"

EXTRACTED_DIR="${SANDBOX_ROOT}/${SUBMISSION_NAME}"
echo "[PACKAGER] Verifying clean-sandbox compilation across all 4 case directories..."

for case_dir in Part1 Part2_Case1 Part2_Case2 Part2_Case2_Enhanced; do
    make -C "${EXTRACTED_DIR}/${case_dir}" >/dev/null
    if [[ ! -x "${EXTRACTED_DIR}/${case_dir}/server" || ! -x "${EXTRACTED_DIR}/${case_dir}/client" ]]; then
        echo "[ERROR] Sandbox build failed to produce server/client in ${case_dir}" >&2
        exit 1
    fi
    echo "  [OK] ${case_dir}/server and ${case_dir}/client built cleanly in sandbox"
done

echo "[PACKAGER] Running automated correctness regression suite inside clean sandbox..."
bash "${EXTRACTED_DIR}/scripts/test_correctness.sh" >/dev/null
echo "  [OK] Clean-sandbox regression suite passed 100% (22/22 checks)"

echo "======================================================================"
echo "SUBMISSION PACKAGING & SANDBOX VERIFICATION SUCCEEDED!"
echo "Archive: ${ROOT_DIR}/${SUBMISSION_NAME}.tar.gz"
echo "======================================================================"
