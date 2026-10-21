#!/usr/bin/env python3
"""Experiments 2/3/4 - Part II fault-injection sweeps.

  exp2 : Case 1 (no session management / restart from beginning)
  exp3 : Case 2 (session management / checkpoint resume)
  exp4 : Case 2 Enhanced (parallel TCP connections, N = 1,2,4)

Usage:  bench_exp_faults.py exp2|exp3|exp4

For each configuration: one warm-up run (repetition 0, is_warmup=1) followed by
IT305_REPS measured repetitions. A fresh server process is started for each run
(this is also REQUIRED for the Enhanced server, which keeps a single global
session).  The client is invoked with the standard CLI; Enhanced additionally
receives the optional [num_connections] argument.

Metric definitions (all application-level, i.e. protocol frames, no TCP/IP
headers):
  useful_bytes             verified (SHA-256, byte-exact) file-content bytes
  data_payload_received    file-content bytes carried by every MSG_FILE_DATA /
                           MSG_WORK_DATA frame the client received (all attempts)
  redundant_payload_bytes  data_payload_received - useful_bytes
  total_transmitted_bytes  every byte sent + received by the client process
                           (frame headers, control payloads, data payloads,
                           including retries)
  protocol_control_bytes   total_transmitted - data_payload_received
  byte_overhead_pct        (total_transmitted - useful) / useful * 100
                           (redundant payload is inside total exactly once)
  useful_throughput        useful_bytes / elapsed (monotonic, first spawn ->
                           client exit; includes retry back-off and reconnects)
"""
import os
import re
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench_lib import *  # noqa

exp = sys.argv[1]
TOPICS_ROOT = Path(os.environ["IT305_TOPICS_ROOT"])
WORK = Path(os.environ.get("IT305_WORK", Path.home() / "it305" / "work"))
REPS = int(os.environ.get("IT305_REPS", "5"))
PROBS = [x for x in os.environ.get("IT305_PROBS", "0.00,0.01,0.05,0.10").split(",")]
MCFG = machine_configuration()
MANIFEST = RESULTS / "benchmark_run_manifest.csv"

CFG = {
    "exp2": dict(impl="case1", raw="exp2_case1_raw.csv", log="exp2_case1.log",
                 label="Case1", timeout=600,
                 sweeps=[("gatto_fault20", PROBS, [None])]),
    "exp3": dict(impl="case2", raw="exp3_case2_raw.csv", log="exp3_case2.log",
                 label="Case2", timeout=300,
                 sweeps=[("gatto_fault20", PROBS, [None]),
                         ("gatto_big150", ["0.00", "0.05"], [None])]),
    "exp4": dict(impl="enhanced", raw="exp4_enhanced_raw.csv", log="exp4_enhanced.log",
                 label="Case2_Enhanced", timeout=300,
                 sweeps=[("gatto_fault20", ["0.00", "0.05"], ["default", 1, 2, 4]),
                         ("gatto_big150", ["0.00", "0.05"], [1, 2, 4])]),
}[exp]
LOGS = Path(os.environ.get("IT305_RUNLOGS", Path.home() / "it305" / "run_logs")) / exp
RAW, LOG = RESULTS / CFG["raw"], RESULTS / CFG["log"]

FIELDS = ["run_id", "timestamp", "dataset", "topic", "implementation", "failure_probability",
          "num_connections", "cli_form", "repetition", "is_warmup", "useful_bytes",
          "total_transmitted_bytes", "data_payload_bytes_received",
          "redundant_payload_bytes", "protocol_control_bytes", "elapsed_seconds",
          "useful_throughput_bytes_per_sec", "byte_overhead_pct", "client_attempts",
          "reconnects", "faults_injected_server", "client_exit_code", "timed_out",
          "integrity_pass", "failure_note", "machine_configuration"]
MFIELDS = ["run_id", "experiment", "configuration", "raw_data_file", "log_location",
           "integrity_outcome", "warmup"]

log_line(LOG, f"{exp} start impl={CFG['impl']} sweeps={CFG['sweeps']} reps={REPS}+1 warm-up")
log_line(LOG, f"machine: {MCFG}")

for topic, probs, conns in CFG["sweeps"]:
    src_topic = TOPICS_ROOT / topic
    src = source_manifest(src_topic)
    topic_bytes = sum(v[0] for v in src.values())
    log_line(LOG, f"topic={topic}: {len(src)} files, {topic_bytes} bytes")
    for p in probs:
        for n in conns:
            for rep in range(0, REPS + 1):
                n_label = "" if n is None else (1 if n == "default" else n)
                cli_form = "default(no optional arg)" if n in (None, "default") else f"optional arg N={n}"
                run_id = (f"{exp}-{topic}-p{p}-n{n_label if n is not None else '-'}"
                          f"{'d' if n == 'default' else ''}-r{rep}-{datetime.datetime.now():%H%M%S%f}")
                logdir = LOGS / run_id
                outdir = WORK / run_id
                shutil.rmtree(outdir, ignore_errors=True)
                port = free_port()
                srv = Server(CFG["impl"], port, TOPICS_ROOT, p, logdir)
                extra = [n] if isinstance(n, int) else []
                c = spawn_client(CFG["impl"], port, topic, outdir, extra, logdir, "client")
                finish_client(c, timeout=CFG["timeout"])
                srv_err = srv.stop()
                elapsed = (c["t1"] - c["t0"]) / 1e9
                ver = verify_download(src_topic, outdir / topic)
                ok = c["rc"] == 0 and not c["timed_out"] and ver["ok"]
                note = ""
                if not ok:
                    note = (f"rc={c['rc']} timed_out={c['timed_out']} verify='{ver['reason']}'")
                useful = ver["bytes"] if ok else 0
                m = c["metrics"]
                total = (m[0] + m[1]) if m else 0
                data_recv = m[3] if m else 0
                redundant = max(0, data_recv - useful) if ok else ""
                control = (total - data_recv) if m else ""
                overhead = (f"{(total - useful) / useful * 100:.4f}" if (ok and useful > 0) else "")
                attempts = len(re.findall(r"Transfer Attempt #", c["stdout_text"]))
                if CFG["impl"] == "enhanced":
                    reconnects = c["stderr_text"].count("reconnecting")
                    attempts = ""
                else:
                    reconnects = max(0, attempts - 1)
                row = {
                    "run_id": run_id, "timestamp": now_iso(),
                    "dataset": "Animals-10 (real images, subset/topic staged from repo raw-img)",
                    "topic": topic, "implementation": CFG["label"],
                    "failure_probability": p,
                    "num_connections": 1 if n in (None, "default") else n,
                    "cli_form": cli_form, "repetition": rep, "is_warmup": int(rep == 0),
                    "useful_bytes": useful, "total_transmitted_bytes": total,
                    "data_payload_bytes_received": data_recv,
                    "redundant_payload_bytes": redundant, "protocol_control_bytes": control,
                    "elapsed_seconds": f"{elapsed:.6f}",
                    "useful_throughput_bytes_per_sec": f"{safe_div(useful, elapsed):.1f}",
                    "byte_overhead_pct": overhead, "client_attempts": attempts,
                    "reconnects": reconnects, "faults_injected_server": srv.fault_count,
                    "client_exit_code": c["rc"], "timed_out": int(c["timed_out"]),
                    "integrity_pass": int(ok), "failure_note": note,
                    "machine_configuration": MCFG,
                }
                append_rows(RAW, FIELDS, [row])
                append_rows(MANIFEST, MFIELDS, [{
                    "run_id": run_id, "experiment": exp,
                    "configuration": (f"impl={CFG['label']};topic={topic};p={p};"
                                      f"N={row['num_connections']};cli={cli_form};rep={rep}"),
                    "raw_data_file": CFG["raw"],
                    "log_location": f"run_logs_{exp}.tar.gz:{exp}/{run_id}/",
                    "integrity_outcome": "PASS" if ok else "FAIL", "warmup": int(rep == 0)}])
                log_line(LOG, f"{run_id} rep={rep}{' (warm-up)' if rep == 0 else ''} "
                              f"elapsed={elapsed:.3f}s useful={useful}B "
                              f"thr={safe_div(useful, elapsed) / 1e6:.3f}MB/s "
                              f"total_tx={total}B redundant={redundant}B control={control}B "
                              f"overhead={overhead}% faults={srv.fault_count} "
                              f"reconnects={reconnects} integrity={'PASS' if ok else 'FAIL'} {note}")
                shutil.rmtree(outdir, ignore_errors=True)
log_line(LOG, f"{exp} complete")
