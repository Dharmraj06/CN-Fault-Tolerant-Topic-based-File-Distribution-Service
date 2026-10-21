#!/usr/bin/env python3
"""Experiment 1 - Part I: sequential vs multi-threaded server, 1/2/4/8 concurrent clients.

For every (mode, client_count) one warm-up run (repetition 0, flagged is_warmup=1)
and REPS measured repetitions are executed.  A FRESH server process is started
for every run.  All clients of a run are launched back-to-back (concurrent
launch); the experiment wall-clock covers first client spawn -> last client exit.
Every client's download is verified byte-for-byte (SHA-256) before it counts.
"""
import os
import shutil
import sys
import threading
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench_lib import *  # noqa

TOPICS_ROOT = Path(os.environ["IT305_TOPICS_ROOT"])
WORK = Path(os.environ.get("IT305_WORK", Path.home() / "it305" / "work"))
LOGS = Path(os.environ.get("IT305_RUNLOGS", Path.home() / "it305" / "run_logs")) / "exp1"
TOPIC = os.environ.get("IT305_EXP1_TOPIC", "mucca")
REPS = int(os.environ.get("IT305_REPS", "5"))
COUNTS = [int(x) for x in os.environ.get("IT305_EXP1_COUNTS", "1,2,4,8").split(",")]
MODES = os.environ.get("IT305_EXP1_MODES", "sequential,mt").split(",")
RAW = RESULTS / "exp1_part1_raw.csv"
LOG = RESULTS / "exp1_part1.log"
MANIFEST = RESULTS / "benchmark_run_manifest.csv"
MCFG = machine_configuration()
DATASET = "Animals-10 (real images, repo raw-img/mucca, complete topic)"

FIELDS = ["run_id", "timestamp", "dataset", "topic", "server_mode", "client_count",
          "repetition", "is_warmup", "total_useful_bytes", "total_transmitted_bytes",
          "elapsed_seconds", "aggregate_useful_throughput_bytes_per_sec",
          "individual_client_completion_times", "failures", "integrity_pass",
          "total_data_payload_bytes_received", "server_data_payload_bytes_sent",
          "server_wire_bytes_sent", "server_max_threads_observed",
          "client_intervals_overlap", "per_client_start_offsets_s",
          "per_client_exit_codes", "machine_configuration"]
MFIELDS = ["run_id", "experiment", "configuration", "raw_data_file", "log_location",
           "integrity_outcome", "warmup"]

src_topic = TOPICS_ROOT / TOPIC
source_manifest(src_topic)  # hash source once
topic_bytes = sum(v[0] for v in source_manifest(src_topic).values())
log_line(LOG, f"EXP1 start: topic={TOPIC} files={len(source_manifest(src_topic))} "
              f"bytes={topic_bytes} modes={MODES} counts={COUNTS} reps={REPS}+1 warm-up")
log_line(LOG, f"machine: {MCFG}")

for mode in MODES:
    for n in COUNTS:
        for rep in range(0, REPS + 1):
            run_id = f"exp1-{mode}-c{n}-r{rep}-{datetime.datetime.now():%H%M%S%f}"
            logdir = LOGS / run_id
            outbase = WORK / run_id
            shutil.rmtree(outbase, ignore_errors=True)
            port = free_port()
            srv = Server("part1", port, TOPICS_ROOT, mode, logdir)
            clients, threads = [], []
            t_exp0 = time.monotonic_ns()
            for i in range(n):
                clients.append(spawn_client("part1", port, TOPIC, outbase / f"c{i}", [],
                                            logdir, f"client{i}"))
            # wait for each client in its own thread so exit times are exact
            def waiter(c):
                finish_client(c, timeout=600)
            for c in clients:
                t = threading.Thread(target=waiter, args=(c,))
                t.start()
                threads.append(t)
            for t in threads:
                t.join()
            t_exp1 = max(c["t1"] for c in clients)
            srv.stop()
            elapsed = (t_exp1 - t_exp0) / 1e9
            ver = [verify_download(src_topic, outbase / f"c{i}" / TOPIC) for i in range(n)]
            ok = [c["rc"] == 0 and not c["timed_out"] and v["ok"] for c, v in zip(clients, ver)]
            useful = sum(v["bytes"] for v, k in zip(ver, ok) if k)
            transmitted = sum((c["metrics"][0] + c["metrics"][1]) if c["metrics"] else 0
                              for c in clients)
            data_recv = sum(c["metrics"][3] if c["metrics"] else 0 for c in clients)
            overlap = max(c["t0"] for c in clients) < min(c["t1"] for c in clients)
            row = {
                "run_id": run_id, "timestamp": now_iso(), "dataset": DATASET, "topic": TOPIC,
                "server_mode": mode, "client_count": n, "repetition": rep,
                "is_warmup": int(rep == 0), "total_useful_bytes": useful,
                "total_transmitted_bytes": transmitted,
                "elapsed_seconds": f"{elapsed:.6f}",
                "aggregate_useful_throughput_bytes_per_sec": f"{safe_div(useful, elapsed):.1f}",
                "individual_client_completion_times": ";".join(
                    f"{(c['t1'] - c['t0']) / 1e9:.4f}" for c in clients),
                "failures": sum(1 for k in ok if not k),
                "integrity_pass": int(all(ok)),
                "total_data_payload_bytes_received": data_recv,
                "server_data_payload_bytes_sent": srv.metrics[2] if srv.metrics else "",
                "server_wire_bytes_sent": srv.metrics[0] if srv.metrics else "",
                "server_max_threads_observed": srv.max_threads,
                "client_intervals_overlap": int(overlap),
                "per_client_start_offsets_s": ";".join(
                    f"{(c['t0'] - t_exp0) / 1e9:.4f}" for c in clients),
                "per_client_exit_codes": ";".join(str(c["rc"]) for c in clients),
                "machine_configuration": MCFG,
            }
            append_rows(RAW, FIELDS, [row])
            append_rows(MANIFEST, MFIELDS, [{
                "run_id": run_id, "experiment": "exp1_part1",
                "configuration": f"mode={mode};clients={n};topic={TOPIC};rep={rep}",
                "raw_data_file": "exp1_part1_raw.csv",
                "log_location": f"run_logs_exp1.tar.gz:exp1/{run_id}/",
                "integrity_outcome": "PASS" if all(ok) else "FAIL",
                "warmup": int(rep == 0)}])
            log_line(LOG, f"{run_id} mode={mode} clients={n} rep={rep}"
                          f"{' (warm-up)' if rep == 0 else ''} elapsed={elapsed:.3f}s "
                          f"useful={useful}B throughput={safe_div(useful, elapsed) / 1e6:.2f}MB/s "
                          f"integrity={'PASS' if all(ok) else 'FAIL'} "
                          f"overlap={overlap} server_threads_max={srv.max_threads} "
                          f"rc={[c['rc'] for c in clients]} "
                          f"{[v['reason'] for v in ver if not v['ok']]}")
            shutil.rmtree(outbase, ignore_errors=True)
log_line(LOG, "EXP1 complete")
