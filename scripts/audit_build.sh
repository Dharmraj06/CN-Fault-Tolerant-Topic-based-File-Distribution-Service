#!/bin/bash
# Clean-build all four required implementations and capture evidence.
# Usage: bash scripts/audit_build.sh <repo_root> <log_dir>
ROOT="${1:-.}"
LOGDIR="${2:-$ROOT/Results/build_logs}"
mkdir -p "$LOGDIR"
SUMMARY="$LOGDIR/build_summary.txt"
: > "$SUMMARY"
cd "$ROOT" || exit 1
for d in Part1 Part2_Case1 Part2_Case2 Part2_Case2_Enhanced; do
    {
        echo "=== $d: make clean ($(date -Is)) ==="
        make -C "$d" clean 2>&1; echo "exit=$?"
        echo "=== $d: make ($(date -Is)) ==="
        make -C "$d" 2>&1; echo "exit=$?"
    } > "$LOGDIR/${d}_build.log" 2>&1
    rc_make=$(grep -c '^exit=0' "$LOGDIR/${d}_build.log")
    warn=$(grep -ci 'warning' "$LOGDIR/${d}_build.log")
    {
        echo "[$d] exits_ok=$rc_make/2 warnings=$warn"
        ls -l --time-style=full-iso "$d/server" "$d/client" 2>&1
        file "$d/server" "$d/client"
        echo "-- second make (expect nothing to do):"
        make -C "$d" 2>&1 | tail -3
    } >> "$SUMMARY"
done
cat "$SUMMARY"
