#!/bin/bash
# Fast checks: error handling plus the first seconds of a real cane session.
PORT=${PORT:-5300}
B=./Part2_Case2_Enhanced
OUT=$(mktemp -d "$HOME/cn-quick.XXXX")
$B/server $PORT "$PWD/raw-img" 0.0 >"$OUT/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null' EXIT
sleep 1

echo "-- invalid topic"
timeout 20 $B/client 127.0.0.1 $PORT nosuch "$OUT/dx" 1; echo "exit=$?"

echo "-- invalid connection count (99)"
python3 - $PORT <<'EOF'
import socket, struct, sys
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=5)
p = b"cane".ljust(128, b"\0") + struct.pack("!I", 99)
s.sendall(struct.pack("!HBBHHQ", 0x5446, 1, 8, 0, 0, len(p)) + p)
print("reply type:", s.recv(16)[3], "(30 = MSG_ERROR)")
EOF

echo "-- truncated handshake then close"
python3 - $PORT <<'EOF'
import socket, struct, sys
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])))
s.sendall(struct.pack("!HBBHHQ", 0x5446, 1, 8, 0, 0, 132) + b"cane")
s.close()
EOF
sleep 1

echo "-- real cane session (30s window)"
timeout 30 $B/client 127.0.0.1 $PORT cane "$OUT/d5" 1 2>&1 | head -4
echo "files so far: $(ls "$OUT/d5/cane" | wc -l)"

echo "-- server log"
head -40 "$OUT/server.log"
