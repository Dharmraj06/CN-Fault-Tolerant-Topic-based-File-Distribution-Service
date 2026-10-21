#!/usr/bin/env python3
"""Supplementary correctness audit (complements the repository's own test suites).

Runs against the compiled binaries of all four variants and records PASS/FAIL
with concrete evidence in Results/correctness_extra_results.csv and
Results/correctness_extra.log.  Failures are recorded, never suppressed.
"""
import csv
import os
import random
import shutil
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench_lib import *  # noqa

FIX = Path(os.environ.get("IT305_FIXTURES", Path.home() / "it305" / "fixtures"))
WORK = Path.home() / "it305" / "work_audit"
LOGD = Path.home() / "it305" / "run_logs" / "audit"
BENCH = Path(os.environ["IT305_TOPICS_ROOT"])
OUTCSV = RESULTS / "correctness_extra_results.csv"
OUTLOG = RESULTS / "correctness_extra.log"
rows = []


def check(test, cond, detail, status=None):
    st = status or ("PASS" if cond else "FAIL")
    rows.append({"test": test, "status": st, "evidence": str(detail)[:600]})
    log_line(OUTLOG, f"[{st}] {test} :: {detail}")


# ------------------------------------------------------------------ fixtures
def make_fixtures():
    shutil.rmtree(FIX, ignore_errors=True)
    rnd = random.Random(1234)
    topics = {
        "bin_topic": {
            "zero.bin": b"",
            "one.bin": b"\x00",
            "allbytes.bin": bytes(range(256)) * 37,
            "exact64k.bin": rnd.randbytes(65536),
            "plus1.bin": rnd.randbytes(65537),
            "rand3m.bin": rnd.randbytes(3 * 1024 * 1024),
        },
        "multi_topic": {f"f{i:03d}.dat": rnd.randbytes(rnd.randint(1, 200000)) for i in range(25)},
        "nozero_topic": {"a.bin": rnd.randbytes(100000), "b.bin": rnd.randbytes(5)},
    }
    for t, files in topics.items():
        d = FIX / t
        d.mkdir(parents=True)
        for n, b in files.items():
            (d / n).write_bytes(b)
    (FIX / "empty_topic").mkdir()


def run(cmd, timeout=30, env=None):
    try:
        p = subprocess.run(cmd, capture_output=True, timeout=timeout, env=env)
        return p.returncode, (p.stdout + p.stderr).decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        return "TIMEOUT", ""


def bin_(impl, which):
    return str(REPO / IMPL_DIR[impl] / which)


def one_transfer(impl, topic, root, prob, extra=(), timeout=60, tag="t"):
    port = free_port()
    arg = "mt" if impl == "part1" else prob
    srv = Server(impl, port, root, arg, LOGD / f"{impl}_{tag}")
    out = WORK / f"{impl}_{tag}"
    shutil.rmtree(out, ignore_errors=True)
    c = spawn_client(impl, port, topic, out, list(extra), LOGD / f"{impl}_{tag}", "client")
    finish_client(c, timeout)
    err = srv.stop()
    return c, srv, out, err


# ------------------------------------------------------------------ frames
def frame(t, payload=b"", flags=0):
    return struct.pack(">HBBHHQ", 0x5446, 1, t, flags, 0, len(payload)) + payload


def read_exact(s, n):
    b = b""
    while len(b) < n:
        x = s.recv(n - len(b))
        if not x:
            raise EOFError
        b += x
    return b


def read_frame(s):
    h = read_exact(s, 16)
    magic, ver, t, flags, res, ln = struct.unpack(">HBBHHQ", h)
    return t, flags, read_exact(s, ln) if ln else b""


def main():
    OUTLOG.write_text("")
    log_line(OUTLOG, "supplementary correctness audit start; " + machine_configuration())
    make_fixtures()
    impls = ["part1", "case1", "case2", "enhanced"]

    # ---- 1. CLI interface -------------------------------------------------
    for impl in impls:
        srv = bin_(impl, "server")
        rc, _ = run([srv])
        check(f"{impl}: server with no args rejected", rc not in (0, "TIMEOUT"), f"rc={rc}")
        for badport in ("abc", "0", "70000"):
            args = [srv, badport, str(FIX)] + ([] if impl == "part1" else ["0.1"])
            rc, _ = run(args, timeout=5)
            check(f"{impl}: server invalid port '{badport}' rejected", rc not in (0, "TIMEOUT"), f"rc={rc}")
        if impl != "part1":
            for badp in ("abc", "-0.1", "1.5", "nan", "0.1x"):
                rc, _ = run([srv, "31999", str(FIX), badp], timeout=5)
                check(f"{impl}: server invalid failure probability '{badp}' rejected",
                      rc not in (0, "TIMEOUT"), f"rc={rc}")
            rc, _ = run([srv, "31999", str(FIX)], timeout=5)
            check(f"{impl}: server without failure probability rejected", rc not in (0, "TIMEOUT"), f"rc={rc}")
        rc, _ = run([srv, "31998", "/nonexistent_dir_xyz"] + ([] if impl == "part1" else ["0.1"]), timeout=5)
        check(f"{impl}: server nonexistent topics root rejected", rc not in (0, "TIMEOUT"), f"rc={rc}")
        cl = bin_(impl, "client")
        rc, _ = run([cl])
        check(f"{impl}: client with no args rejected", rc not in (0, "TIMEOUT"), f"rc={rc}")
        rc, _ = run([cl, "999.1.1.1", "31999", "t", str(WORK)], timeout=10)
        check(f"{impl}: client invalid IP rejected/failed", rc not in (0, "TIMEOUT"), f"rc={rc}")
        rc, _ = run([cl, "127.0.0.1", "abc", "t", str(WORK)], timeout=10)
        check(f"{impl}: client invalid port rejected/failed", rc not in (0, "TIMEOUT"), f"rc={rc}")
        rc, _ = run([cl, "127.0.0.1", "1", "t", str(WORK)], timeout=15)
        check(f"{impl}: client connection refused terminates (no hang)", rc != "TIMEOUT", f"rc={rc}")

    # ---- 2. binary / edge-case transfers with SHA-256 -----------------------
    for impl in impls:
        for topic in ("bin_topic", "multi_topic", "nozero_topic", "empty_topic"):
            c, srv, out, _ = one_transfer(impl, topic, FIX, "0.0", tag=topic)
            v = verify_download(FIX / topic, out / topic)
            exp_ok = True
            check(f"{impl}: topic '{topic}' transfer + SHA-256 byte-exact",
                  c["rc"] == 0 and v["ok"],
                  f"rc={c['rc']} timed_out={c['timed_out']} files={v['files']} bytes={v['bytes']} {v['reason']}")
            if topic == "bin_topic":
                check(f"{impl}: output saved under <output_dir>/<topic>/",
                      (out / topic).is_dir(), str(out / topic))

    # ---- 3. unknown topic, server stays alive --------------------------------
    for impl in impls:
        port = free_port()
        arg = "mt" if impl == "part1" else "0.0"
        srv = Server(impl, port, FIX, arg, LOGD / f"{impl}_unknown")
        o1 = WORK / f"{impl}_unk1"
        c1 = spawn_client(impl, port, "no_such_topic", o1, [], LOGD / f"{impl}_unknown", "c_bad")
        finish_client(c1, 60)
        o2 = WORK / f"{impl}_unk2"
        c2 = spawn_client(impl, port, "multi_topic", o2, [], LOGD / f"{impl}_unknown", "c_good")
        finish_client(c2, 60)
        srv.stop()
        v = verify_download(FIX / "multi_topic", o2 / "multi_topic")
        check(f"{impl}: unknown topic -> client error exit, no hang",
              c1["rc"] not in (0,) and not c1["timed_out"], f"rc={c1['rc']} timed_out={c1['timed_out']}")
        check(f"{impl}: server keeps serving after an unknown-topic request",
              c2["rc"] == 0 and v["ok"], f"second client rc={c2['rc']} {v['reason']}")

    # ---- 4. Case 2 session protocol edge cases (raw wire client) -------------
    port = free_port()
    srv = Server("case2", port, FIX, "0.0", LOGD / "case2_raw")
    topic = b"multi_topic".ljust(128, b"\0")
    s = socket.create_connection(("127.0.0.1", port))
    s.sendall(frame(0x08, topic + struct.pack(">I", 1)))
    t, fl, pl = read_frame(s)
    sid, fp, fc = struct.unpack(">QII", pl)
    check("case2: SESSION_START -> SESSION_ACCEPT with non-zero id", t == 0x09 and sid != 0,
          f"type=0x{t:02x} sid=0x{sid:016x} files={fc}")
    t2, _, _ = read_frame(s)  # FILE_BEGIN
    t3, _, dpl = read_frame(s)  # first FILE_DATA
    first_len = len(dpl)
    # ack first chunk (end_offset=first_len) -> server commits checkpoint
    s.sendall(frame(0x0A, struct.pack(">QIQI", sid, 0, first_len, 1)))
    time.sleep(0.3)
    s.close()  # abrupt client-side disconnect mid transfer
    time.sleep(0.3)

    def resume(sid_, fp_, idx, off, topic_=topic):
        r = socket.create_connection(("127.0.0.1", port))
        r.sendall(frame(0x0B, struct.pack(">Q128sIIQ", sid_, topic_, fp_, idx, off)))
        try:
            t_, f_, p_ = read_frame(r)
        except EOFError:
            r.close()
            return None, 0, b""
        r.close()
        return t_, f_, p_

    t_, f_, p_ = resume(sid, fp, 0, first_len)
    auth = struct.unpack(">QIQH", p_) if t_ == 0x0C else None
    check("case2: valid resume accepted at authoritative checkpoint",
          t_ == 0x0C and auth[1] == 0 and auth[2] == first_len, f"type={t_} resp={auth}")
    t_, f_, p_ = resume(sid, fp, 0, first_len + 40000)  # client claims more than server committed
    auth = struct.unpack(">QIQH", p_) if t_ == 0x0C else None
    check("case2: client/server offset disagreement -> server's authoritative offset wins",
          t_ == 0x0C and auth[2] == first_len, f"client proposed {first_len + 40000}; response={auth} flags=0x{f_:04x}")
    t_, f_, p_ = resume(sid, fp ^ 0xFFFF, 0, 0)
    check("case2: manifest fingerprint mismatch rejected (RESUME_ERROR)", t_ == 0x0D,
          f"type={t_} code={struct.unpack('>I', p_[:4])[0] if p_ else None}")
    t_, f_, p_ = resume(0xDEADBEEFCAFE1234, fp, 0, 0)
    check("case2: unknown/stale session id rejected (RESUME_ERROR)", t_ == 0x0D,
          f"type={t_} msg={p_[4:40] if p_ else None}")
    srv.stop()
    # server restart => in-memory sessions are lost
    srv2 = Server("case2", port, FIX, "0.0", LOGD / "case2_raw2")
    t_, f_, p_ = resume(sid, fp, 0, first_len)
    check("case2: session state does NOT survive a server-process restart (in-memory only)",
          t_ == 0x0D, f"after restart resume reply type=0x{t_:02x} -> RESUME_ERROR expected; "
                      "documented limitation: sessions survive connection loss only", )
    srv2.stop()

    # ---- 5. Case 1 restart-from-beginning / Case 2 resume evidence ------------
    c, srv, out, err = one_transfer("case1", "gatto_fault20", BENCH, "0.10", timeout=300, tag="c1evid")
    v = verify_download(BENCH / "gatto_fault20", out / "gatto_fault20")
    txt = c["stdout_text"]
    attempts = txt.count("Transfer Attempt #")
    purges = txt.count("Purging incomplete topic directory")
    accepted = txt.count("Server accepted topic")
    check("case1: faults -> purge partial output and restart whole topic from beginning",
          v["ok"] and srv.fault_count == attempts - 1 and purges == attempts - 1 and accepted == attempts,
          f"faults={srv.fault_count} attempts={attempts} purges={purges} fresh topic requests={accepted} integrity={v['ok']}")
    c, srv, out, err = one_transfer("case2", "gatto_fault20", BENCH, "0.10", timeout=300, tag="c2evid")
    v = verify_download(BENCH / "gatto_fault20", out / "gatto_fault20")
    txt = c["stdout_text"]
    import re
    faults = re.findall(r"\[FAULT\].*File '([^']+)' offset (\d+)/(\d+)", err)
    resumes = re.findall(r"Resume ACCEPTED: Authoritative Checkpoint \[file (\d+), offset (\d+) bytes\]", txt)
    same = (len(faults) == len(resumes))
    check("case2: each injected fault followed by session RESUME at the fault offset",
          v["ok"] and same and srv.fault_count == len(resumes),
          f"faults={[(f[0][:12], int(f[1])) for f in faults]} resumes={[(int(a), int(b)) for a, b in resumes]} integrity={v['ok']}")
    m = c["metrics"]
    check("case2: no redundant payload resent after resume",
          v["ok"] and m and m[3] == v["bytes"], f"data_payload_recv={m[3] if m else None} useful={v['bytes']}")

    # ---- 6. Enhanced specifics ---------------------------------------------------
    c, srv, out, err = one_transfer("enhanced", "gatto_fault20", BENCH, "0.05", extra=[4], tag="enh4")
    v = verify_download(BENCH / "gatto_fault20", out / "gatto_fault20")
    workers = sorted(set(re.findall(r"\[worker (\d+)\] connected", err)))
    check("enhanced: 4 worker connections established and bit-exact result (p=0.05)",
          v["ok"] and len(workers) == 4, f"workers={workers} server_max_threads={srv.max_threads} integrity={v['ok']} faults={srv.fault_count}")
    check("enhanced: connections concurrently active (server thread count)",
          srv.max_threads >= 5, f"max server threads observed={srv.max_threads} (1 acceptor + up to 4 workers)")
    port = free_port()
    srv = Server("enhanced", port, BENCH, "0.0", LOGD / "enh_two")
    o1, o2 = WORK / "enh_s1", WORK / "enh_s2"
    c1 = spawn_client("enhanced", port, "gatto_fault20", o1, [], LOGD / "enh_two", "first")
    finish_client(c1, 60)
    c2 = spawn_client("enhanced", port, "gatto_fault20", o2, [], LOGD / "enh_two", "second")
    finish_client(c2, 20)
    srv.stop()
    v2 = verify_download(BENCH / "gatto_fault20", o2 / "gatto_fault20")
    check("enhanced: SAME server instance serves a SECOND client/session",
          c2["rc"] == 0 and v2["ok"],
          f"first client rc={c1['rc']}; second client rc={c2['rc']} timed_out={c2['timed_out']} verify='{v2['reason']}' "
          "(server keeps one global session: g_plan/g_session_id/first_worker_started)")
    c, srv, out, err = one_transfer("enhanced", "gatto_big150", BENCH, "0.0", extra=[4], timeout=60, tag="enh_big150")
    v = verify_download(BENCH / "gatto_big150", out / "gatto_big150")
    check("enhanced: 150-file real Animals-10 topic (gatto_big150) transfers bit-exact with N=4",
          c["rc"] == 0 and v["ok"],
          f"rc={c['rc']} timed_out={c['timed_out']} files={v['files']} bytes={v['bytes']} verify='{v['reason']}'")
    c, srv, out, err = one_transfer("enhanced", "mucca", BENCH, "0.0", timeout=30, tag="enh_big")
    check("enhanced: topic with >1024 files (real 'mucca', 1866 files) correctly rejected (capacity limit)",
          c["rc"] != 0 and not c["timed_out"],
          f"rc={c['rc']} timed_out={c['timed_out']} (rejected by design ceiling MAX_WORK_UNITS=1024) server_err_tail={err.strip().splitlines()[-1] if err.strip() else ''}")

    # ---- 7. Part1 sequential serialisation vs MT concurrency ---------------------
    for mode in ("sequential", "mt"):
        port = free_port()
        srv = Server("part1", port, FIX, mode, LOGD / f"p1_{mode}")
        cs = [spawn_client("part1", port, "multi_topic", WORK / f"p1_{mode}_{i}", [], LOGD / f"p1_{mode}", f"c{i}") for i in range(8)]
        for x in cs:
            finish_client(x, 120)
        srv.stop()
        ok = all(x["rc"] == 0 and verify_download(FIX / "multi_topic", WORK / f"p1_{mode}_{i}" / "multi_topic")["ok"] for i, x in enumerate(cs))
        check(f"part1 {mode}: 8 simultaneous clients all bit-exact", ok,
              f"server max threads={srv.max_threads}")
        if mode == "mt":
            check("part1 mt: server really serviced connections in separate threads",
                  srv.max_threads >= 2, f"max threads observed={srv.max_threads}")
        else:
            check("part1 sequential: server stayed single-threaded", srv.max_threads == 1,
                  f"max threads observed={srv.max_threads}")

    # ---- 8. hard-coded values scan -----------------------------------------------
    out = subprocess.run(
        "grep -nE '\"/(home|tmp|Users)|127\\.0\\.0\\.1|\\b(8080|9000|5000)\\b' "
        "Part1/*.c Part2_Case1/*.c Part2_Case2/*.c Part2_Case2_Enhanced/*.c Part2_Case2_Enhanced/*.h common/*.c common/*.h | "
        "grep -v 'Usage\\|e\\.g\\.\\|^.*://\\|\\* ' | head -20",
        shell=True, cwd=REPO, capture_output=True, text=True).stdout
    check("no hard-coded IP/port/path in server or client logic (usage text/comments excluded)",
          out.strip() == "", out.strip().replace("\n", " | ") or "no matches")

    fields = ["test", "status", "evidence"]
    with open(OUTCSV, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    npass = sum(r["status"] == "PASS" for r in rows)
    log_line(OUTLOG, f"SUMMARY: {npass} PASS / {len(rows) - npass} FAIL of {len(rows)}")
    print(f"{npass} PASS / {len(rows) - npass} FAIL of {len(rows)}")


if __name__ == "__main__":
    main()
