#!/bin/bash
# Runs every existing repository test suite against a scratch copy of the repo
# (so committed tests/*.log files are never overwritten), then collects the logs.
# Usage: bash scripts/audit_run_tests.sh <repo_root> <scratch_dir> <out_log>
ROOT="$1"; SCR="$2"; OUT="$3"
rm -rf "$SCR"; mkdir -p "$SCR"
( cd "$ROOT" && tar --exclude=raw-img --exclude=_backup_pre_audit --exclude=.git --exclude='Results' -cf - . ) | tar -xf - -C "$SCR"
mkdir -p "$SCR/scripts"
cp "$ROOT/_backup_pre_audit/extracted/scripts/test_correctness.sh" "$SCR/scripts/" 2>/dev/null
cd "$SCR" || exit 1
find . -name '*.sh' -exec sed -i 's/\r$//' {} +
: > "$OUT"
run() {
    local name="$1"; shift
    echo "################ $name  ($(date -Is)) ################" >> "$OUT"
    echo "CMD: $*" >> "$OUT"
    timeout 900 "$@" >> "$OUT" 2>&1
    echo "EXIT_CODE[$name]=$?" >> "$OUT"
    echo >> "$OUT"
}
run run_server_tests.sh bash tests/run_server_tests.sh
run test_phase2.sh      bash tests/test_phase2.sh
run test_phase3.sh      bash tests/test_phase3.sh
run test_phase4.sh      bash tests/test_phase4.sh
run test_correctness.sh bash scripts/test_correctness.sh
echo "ALL_SUITES_DONE" >> "$OUT"
