#!/bin/bash
set -e

PORT=${1:-9020}
TOPIC=${2:-gatto}
CONNS=${3:-1}
OUT_DIR="./test_runs/${TOPIC}_${CONNS}conn"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

echo "=== Starting Enhanced Server on port $PORT (topics root: ./raw-img, fail_prob: 0.0) ==="
./Part2_Case2_Enhanced/server $PORT ./raw-img 0.0 > "./test_runs/server_${TOPIC}_${CONNS}conn.log" 2>&1 &
SERVER_PID=$!

cleanup() {
    echo "Stopping server (PID $SERVER_PID)..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT

sleep 1

echo "=== Running Enhanced Client ($TOPIC, $CONNS conns, target: $OUT_DIR) ==="
START_TIME=$(date +%s%N)
./Part2_Case2_Enhanced/client 127.0.0.1 $PORT "$TOPIC" "$OUT_DIR" "$CONNS"
END_TIME=$(date +%s%N)

ELAPSED_MS=$(( (END_TIME - START_TIME) / 1000000 ))
echo "Transfer elapsed time: ${ELAPSED_MS} ms"

echo "=== Verifying downloaded files count and size ==="
SRC_COUNT=$(ls "./raw-img/$TOPIC" | wc -l)
DST_COUNT=$(ls "$OUT_DIR/$TOPIC" | wc -l)
SRC_BYTES=$(du -sb "./raw-img/$TOPIC" | cut -f1)
DST_BYTES=$(du -sb "$OUT_DIR/$TOPIC" | cut -f1)

echo "Files: source=$SRC_COUNT, dest=$DST_COUNT"
echo "Bytes: source=$SRC_BYTES, dest=$DST_BYTES"

if [ "$SRC_COUNT" -eq "$DST_COUNT" ] && [ "$SRC_BYTES" -eq "$DST_BYTES" ]; then
    echo "SUCCESS: File count and byte size match perfectly!"
else
    echo "FAILURE: Count or size mismatch!"
    exit 1
fi
