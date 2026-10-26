#!/bin/bash
# Quick loopback checks for Part2_Case2_Enhanced (run from project root).
PORT=${PORT:-5200}
OUT=$(mktemp -d)
ROOT="$PWD/raw-img"
B=./Part2_Case2_Enhanced

$B/server $PORT "$ROOT" 0.0 >"$OUT/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$OUT"' EXIT
sleep 1

echo "== 1. valid session, 1 connection (topic cane)"
timeout 300 $B/client 127.0.0.1 $PORT cane "$OUT/d1" 1 >"$OUT/c1.log" 2>&1; echo "exit=$?"
tail -2 "$OUT/c1.log"
echo "files downloaded: $(ls "$OUT/d1/cane" | wc -l) / $(ls "$ROOT/cane" | wc -l)"
(cd "$ROOT/cane" && ls | head -50 | while read f; do cmp -s "$f" "$OUT/d1/cane/$f" || echo "MISMATCH $f"; done); echo "cmp of 50 files done"

echo "== 2. invalid topic"
timeout 20 $B/client 127.0.0.1 $PORT nosuchtopic "$OUT/d2" 1; echo "exit=$?"

echo "== 3. invalid connection count (raw frame, 99 conns)"
python3 - "$PORT" <<'EOF'
import socket, struct, sys
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=5)
payload = b"cane".ljust(128, b"\0") + struct.pack("!I", 99)
s.sendall(struct.pack("!HBBHHQ", 0x5446, 1, 8, 0, 0, len(payload)) + payload)
h = s.recv(16)
print("reply type:", h[3], "(0x1E = MSG_ERROR)")
EOF

echo "== 4. client disconnects mid-handshake, then a new session works"
python3 - "$PORT" <<'EOF'
import socket, struct, sys
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])))
s.sendall(struct.pack("!HBBHHQ", 0x5446, 1, 8, 0, 0, 132) + b"cane")  # truncated payload
s.close()
EOF
timeout 300 $B/client 127.0.0.1 $PORT cane "$OUT/d3" 4 >"$OUT/c3.log" 2>&1; echo "4-conn exit=$?"
echo "files downloaded: $(ls "$OUT/d3/cane" | wc -l)"

echo "== server log"
cat "$OUT/server.log" | head -40
