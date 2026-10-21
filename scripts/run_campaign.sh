#!/bin/bash
# ==============================================================================
# IT305 Computer Networks Lab - Benchmark Campaign Orchestrator
# Executes all required performance experiments and produces raw CSVs,
# derived summaries, run logs, and publication-grade graphs in Results/.
# ==============================================================================
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

RESULTS_DIR="$REPO_DIR/Results"
mkdir -p "$RESULTS_DIR"

TOPICS_ROOT="/home/dharmraj/it305/bench_data"
RUNLOGS_DIR="/home/dharmraj/it305/run_logs"
WORK_DIR="/home/dharmraj/it305/work"

mkdir -p "$RUNLOGS_DIR" "$WORK_DIR"

export IT305_REPO="$REPO_DIR"
export IT305_TOPICS_ROOT="$TOPICS_ROOT"
export IT305_RUNLOGS="$RUNLOGS_DIR"
export IT305_WORK="$WORK_DIR"

CMD_LOG="$RESULTS_DIR/benchmark_commands.log"
: > "$CMD_LOG"

log_cmd() {
    local ts
    ts=$(date -Is)
    echo "[$ts] CMD: $*" >> "$CMD_LOG"
    "$@" 2>&1 | tee -a "$CMD_LOG"
    local rc=${PIPESTATUS[0]}
    echo "[$ts] EXIT_CODE: $rc" >> "$CMD_LOG"
    return $rc
}

echo "=== STEP 1: Recording Benchmark Environment ==="
ENV_FILE="$RESULTS_DIR/benchmark_environment.txt"
{
    echo "================================================================================"
    echo "IT305 BENCHMARK EXECUTION ENVIRONMENT RECORD"
    echo "Timestamp: $(date -Is)"
    echo "================================================================================"
    echo
    echo "--- HOST OPERATING SYSTEM & KERNEL ---"
    uname -a
    cat /etc/os-release
    echo
    echo "--- CPU ARCHITECTURE & HARDWARE DETAILS ---"
    lscpu | grep -E "Model name|Socket|Thread|NUMA|CPU\(s\)|BogoMIPS|Architecture|Byte Order"
    echo
    echo "--- MEMORY CONFIGURATION ---"
    free -m
    echo
    echo "--- COMPILER & TOOLCHAIN VERSIONS ---"
    gcc --version | head -n 2
    make --version | head -n 1
    ld --version | head -n 1
    echo
    echo "--- PYTHON ENVIRONMENT & LIBRARIES ---"
    python3 --version
    python3 -c "import matplotlib, numpy; print('matplotlib:', matplotlib.__version__, 'numpy:', numpy.__version__)"
    echo
    echo "--- NETWORK & LOOPBACK CONFIGURATION ---"
    ip addr show lo
    echo
    echo "--- GIT REPOSITORY STATE ---"
    git rev-parse --show-toplevel
    echo "Branch: $(git branch --show-current)"
    echo "Commit: $(git rev-parse HEAD)"
    echo "Status:"
    git status --short
    echo
    echo "--- DATASET PROVENANCE & LOCATION ---"
    echo "Staged Topics Directory: $TOPICS_ROOT"
    cat "$RESULTS_DIR/dataset_benchmark_topics.csv"
} > "$ENV_FILE"
echo "Saved environment details to $ENV_FILE"

echo "=== STEP 2: Running Experiment 1 (Part I Sequential vs Multi-Threaded) ==="
export IT305_EXP1_TOPIC="mucca"
export IT305_EXP1_MODES="sequential,mt"
export IT305_EXP1_COUNTS="1,2,4,8"
export IT305_REPS=3
log_cmd python3 scripts/bench_exp1_part1.py

echo "=== STEP 3: Running Experiment 2 (Part II Case 1 Full-Restart Fault Tolerance) ==="
export IT305_PROBS="0.00,0.01,0.05,0.10"
export IT305_REPS=5
log_cmd python3 scripts/bench_exp_faults.py exp2

echo "=== STEP 4: Running Experiment 3 (Part II Case 2 Checkpoint Resume) ==="
export IT305_PROBS="0.00,0.01,0.05,0.10"
export IT305_REPS=5
log_cmd python3 scripts/bench_exp_faults.py exp3

echo "=== STEP 5: Running Experiment 4 (Part II Case 2 Enhanced Parallel Connections) ==="
export IT305_REPS=5
log_cmd python3 scripts/bench_exp_faults.py exp4

echo "=== STEP 6: Generating Derived Summaries, Comparison Tables, and Publication Graphs ==="
log_cmd python3 scripts/bench_analyze.py "$RESULTS_DIR"

echo "=== STEP 7: Archiving Run Logs ==="
for e in exp1 exp2 exp3 exp4; do
    if [ -d "$RUNLOGS_DIR/$e" ]; then
        tar -czf "$RESULTS_DIR/run_logs_$e.tar.gz" -C "$RUNLOGS_DIR" "$e"
        echo "Archived $RUNLOGS_DIR/$e to $RESULTS_DIR/run_logs_$e.tar.gz"
    fi
done

echo "================================================================================"
echo "ALL BENCHMARK EXPERIMENTS COMPLETED SUCCESSFULLY!"
echo "================================================================================"
ls -lh "$RESULTS_DIR"
