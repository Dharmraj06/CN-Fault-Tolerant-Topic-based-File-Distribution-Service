#!/usr/bin/env python3
"""Shared helpers for the IT305 benchmark campaign (run inside WSL / Linux).

Every number written by the experiment scripts comes from real executions of the
compiled `server` / `client` binaries.  Elapsed times use time.monotonic_ns()
(CLOCK_MONOTONIC).  Byte counters come from the additive instrumentation in
common/protocol.c (enabled with IT305_METRICS=1) and from re-hashing the files
that were actually written to disk.
"""
import csv
import datetime
import hashlib
import os
import platform
import re
import shutil
import signal
import socket
import subprocess
import threading
import time
from pathlib import Path

REPO = Path(os.environ.get("IT305_REPO", Path(__file__).resolve().parent.parent))
RESULTS = REPO / "Results"
IMPL_DIR = {
    "part1": "Part1",
    "case1": "Part2_Case1",
    "case2": "Part2_Case2",
    "enhanced": "Part2_Case2_Enhanced",
}
CHUNK = 64 * 1024
METRICS_RE = re.compile(
    r"\[METRICS\] wire_bytes_sent=(\d+) wire_bytes_recv=(\d+) "
    r"data_payload_sent=(\d+) data_payload_recv=(\d+)")

_port_lock = threading.Lock()
_port_next = [31000 + (os.getpid() % 1000) * 10]


def now_iso():
    return datetime.datetime.now().astimezone().isoformat(timespec="milliseconds")


def machine_configuration():
    cpu = "unknown"
    try:
        for line in open("/proc/cpuinfo"):
            if line.startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break
    except OSError:
        pass
    mem_kb = 0
    try:
        for line in open("/proc/meminfo"):
            if line.startswith("MemTotal"):
                mem_kb = int(line.split()[1])
                break
    except OSError:
        pass
    return (f"single host (WSL2 Ubuntu, kernel {platform.release()}), {cpu}, "
            f"{os.cpu_count()} logical CPUs, {mem_kb // 1024} MiB RAM, "
            f"loopback 127.0.0.1 (server and all clients on the same machine)")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


_src_cache = {}


def source_manifest(topic_dir):
    """{filename: (size, sha256)} of the source topic (cached)."""
    key = str(topic_dir)
    if key not in _src_cache:
        m = {}
        for name in sorted(os.listdir(topic_dir)):
            p = os.path.join(topic_dir, name)
            if os.path.isfile(p):
                m[name] = (os.path.getsize(p), sha256_file(p))
        _src_cache[key] = m
    return _src_cache[key]


def verify_download(src_topic_dir, out_topic_dir):
    """Byte-exact (SHA-256) comparison of every file. Returns dict."""
    src = source_manifest(src_topic_dir)
    res = {"ok": False, "files": 0, "bytes": 0, "reason": ""}
    if not os.path.isdir(out_topic_dir):
        res["reason"] = "output topic directory missing"
        return res
    got = sorted(os.listdir(out_topic_dir))
    if got != sorted(src.keys()):
        missing = sorted(set(src) - set(got))[:3]
        extra = sorted(set(got) - set(src))[:3]
        res["reason"] = f"file set differs (missing={missing} extra={extra})"
        return res
    total = 0
    for name in got:
        p = os.path.join(out_topic_dir, name)
        size = os.path.getsize(p)
        if size != src[name][0] or sha256_file(p) != src[name][1]:
            res["reason"] = f"content mismatch in {name}"
            return res
        total += size
    res.update(ok=True, files=len(got), bytes=total)
    return res


def port_listening(port):
    hexp = f"{port:04X}"
    for fn in ("/proc/net/tcp", "/proc/net/tcp6"):
        try:
            for line in open(fn).read().splitlines()[1:]:
                f = line.split()
                if f[3] == "0A" and f[1].split(":")[1] == hexp:
                    return True
        except OSError:
            pass
    return False


def free_port():
    with _port_lock:
        while True:
            _port_next[0] += 1
            if _port_next[0] > 60000:
                _port_next[0] = 31000
            p = _port_next[0]
            with socket.socket() as s:
                try:
                    s.bind(("127.0.0.1", p))
                    return p
                except OSError:
                    continue


class Server:
    def __init__(self, impl, port, root, arg, logdir, tag="server"):
        self.impl, self.port, self.root, self.arg = impl, port, str(root), str(arg)
        self.cmd = [str(REPO / IMPL_DIR[impl] / "server"), str(port), self.root, self.arg]
        os.makedirs(logdir, exist_ok=True)
        self.out_path = os.path.join(logdir, f"{tag}.stdout")
        self.err_path = os.path.join(logdir, f"{tag}.stderr")
        env = dict(os.environ, IT305_METRICS="1")
        self.out = open(self.out_path, "wb")
        self.err = open(self.err_path, "wb")
        self.proc = subprocess.Popen(self.cmd, stdout=self.out, stderr=self.err, env=env)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError(f"server exited early rc={self.proc.returncode}: {self.cmd}")
            if port_listening(port):
                break
            time.sleep(0.01)
        else:
            raise RuntimeError("server did not start listening")
        self.max_threads = 1
        self._stop = threading.Event()
        self._mon = threading.Thread(target=self._monitor, daemon=True)
        self._mon.start()

    def _monitor(self):
        path = f"/proc/{self.proc.pid}/task"
        while not self._stop.is_set():
            try:
                self.max_threads = max(self.max_threads, len(os.listdir(path)))
            except OSError:
                return
            time.sleep(0.005)

    def stop(self):
        self._stop.set()
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        self.out.close()
        self.err.close()
        err = open(self.err_path, "rb").read().decode("utf-8", "replace")
        m = METRICS_RE.search(err)
        self.metrics = tuple(int(x) for x in m.groups()) if m else None
        self.fault_count = err.count("[FAULT]")
        return err


def spawn_client(impl, port, topic, outdir, extra, logdir, tag):
    os.makedirs(outdir, exist_ok=True)
    os.makedirs(logdir, exist_ok=True)
    cmd = [str(REPO / IMPL_DIR[impl] / "client"), "127.0.0.1", str(port), topic, str(outdir)]
    cmd += [str(x) for x in extra]
    out = open(os.path.join(logdir, f"{tag}.stdout"), "wb")
    err = open(os.path.join(logdir, f"{tag}.stderr"), "wb")
    env = dict(os.environ, IT305_METRICS="1")
    t0 = time.monotonic_ns()
    proc = subprocess.Popen(cmd, stdout=out, stderr=err, env=env, start_new_session=True)
    return {"proc": proc, "t0": t0, "out": out, "err": err, "cmd": cmd,
            "out_path": out.name, "err_path": err.name, "tag": tag}


def finish_client(c, timeout):
    timed_out = False
    try:
        c["proc"].wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        try:
            os.killpg(c["proc"].pid, signal.SIGKILL)
        except OSError:
            pass
        c["proc"].wait()
    c["t1"] = time.monotonic_ns()
    c["rc"] = c["proc"].returncode
    c["timed_out"] = timed_out
    c["out"].close()
    c["err"].close()
    err = open(c["err_path"], "rb").read().decode("utf-8", "replace")
    out = open(c["out_path"], "rb").read().decode("utf-8", "replace")
    m = METRICS_RE.search(err)
    c["metrics"] = tuple(int(x) for x in m.groups()) if m else None
    c["stdout_text"], c["stderr_text"] = out, err
    return c


def append_rows(path, fieldnames, rows):
    path = Path(path)
    new = not path.exists()
    with open(path, "a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        if new:
            w.writeheader()
        for r in rows:
            w.writerow(r)


def log_line(path, text):
    with open(path, "a") as f:
        f.write(f"[{now_iso()}] {text}\n")


def safe_div(a, b):
    return a / b if b else 0.0
