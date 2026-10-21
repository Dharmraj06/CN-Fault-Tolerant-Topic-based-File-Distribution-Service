#!/usr/bin/env python3
"""Derive summary CSVs, the Case 1 vs Case 2 comparison, and all graphs from the
raw per-run CSVs in Results/.  Nothing here is hard-coded: every plotted point is
computed from the raw observations (warm-up rows excluded, failed runs counted
separately).  The exact plotted points are written to Results/plotdata_*.csv.

Error bars are +/- 1 sample standard deviation over the measured repetitions.
Throughput unit: MB/s with 1 MB = 10^6 bytes.
"""
import csv
import math
import statistics as st
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

R = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "Results"
MB = 1e6


def read(name):
    p = R / name
    if not p.exists():
        return []
    with open(p, newline="") as f:
        return list(csv.DictReader(f))


def fl(x):
    return float(x) if x not in ("", None) else float("nan")


def stats(vals):
    vals = [v for v in vals if not math.isnan(v)]
    if not vals:
        return (float("nan"),) * 4
    sd = st.stdev(vals) if len(vals) > 1 else 0.0
    return st.mean(vals), st.median(vals), sd, len(vals)


def write(name, header, rows):
    with open(R / name, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def fmt(x, nd=6):
    return "" if (isinstance(x, float) and math.isnan(x)) else (f"{x:.{nd}f}" if isinstance(x, float) else x)


def measured(rows):
    return [r for r in rows if r["is_warmup"] == "0"]


STYLE = {"font.size": 12, "axes.grid": True, "grid.alpha": 0.3, "figure.dpi": 130}
plt.rcParams.update(STYLE)


def save(fig, base):
    for ext in ("png", "pdf"):
        fig.savefig(R / f"{base}.{ext}", bbox_inches="tight")
    plt.close(fig)


# ============================================================ Experiment 1
exp1 = read("exp1_part1_raw.csv")
sum1 = {}
if exp1:
    g = defaultdict(list)
    for r in measured(exp1):
        g[(r["topic"], r["server_mode"], int(r["client_count"]))].append(r)
    rows = []
    for (topic, mode, n), rs in sorted(g.items(), key=lambda kv: (kv[0][0], kv[0][1], kv[0][2])):
        ok = [r for r in rs if r["integrity_pass"] == "1"]
        thr = [fl(r["aggregate_useful_throughput_bytes_per_sec"]) for r in ok]
        el = [fl(r["elapsed_seconds"]) for r in ok]
        tx = [fl(r["total_transmitted_bytes"]) for r in ok]
        use = [fl(r["total_useful_bytes"]) for r in ok]
        m_thr, md_thr, sd_thr, k = stats(thr)
        m_el, md_el, sd_el, _ = stats(el)
        ovh = [(t - u) / u * 100 for t, u in zip(tx, use) if u > 0]
        sum1[(topic, mode, n)] = dict(mean=m_thr, sd=sd_thr, n=k)
        rows.append([topic, mode, n, len(rs), k, len(rs) - k, fmt(m_thr, 1), fmt(md_thr, 1), fmt(sd_thr, 1),
                     fmt(m_el, 6), fmt(md_el, 6), fmt(sd_el, 6), fmt(stats(use)[0], 1), fmt(stats(tx)[0], 1),
                     fmt(stats(ovh)[0], 4), max(int(r["server_max_threads_observed"]) for r in rs),
                     min(int(r["client_intervals_overlap"]) for r in rs)])
    # scaling relative to 1 client within the same mode
    hdr = ["topic", "server_mode", "client_count", "measured_runs", "successful_runs", "failed_runs",
           "mean_agg_useful_throughput_Bps", "median_agg_useful_throughput_Bps", "sd_agg_useful_throughput_Bps",
           "mean_elapsed_s", "median_elapsed_s", "sd_elapsed_s", "mean_total_useful_bytes",
           "mean_total_transmitted_bytes", "mean_byte_overhead_pct", "max_server_threads_observed",
           "all_client_intervals_overlap", "scaling_vs_1_client", "mt_over_sequential_ratio"]
    out = []
    for r in rows:
        topic, mode, n = r[0], r[1], r[2]
        base = sum1.get((topic, mode, 1), {}).get("mean", float("nan"))
        cur = sum1[(topic, mode, n)]["mean"]
        seq = sum1.get((topic, "sequential", n), {}).get("mean", float("nan"))
        mt = sum1.get((topic, "mt", n), {}).get("mean", float("nan"))
        out.append(r + [fmt(cur / base if base == base and base else float("nan"), 4),
                        fmt(mt / seq if seq == seq and seq else float("nan"), 4) if mode == "mt" else ""])
    write("exp1_part1_summary.csv", hdr, out)

    topics = sorted({k[0] for k in sum1})
    for topic in topics:
        fig, ax = plt.subplots(figsize=(7.5, 5))
        pdata = []
        for mode, lab, mk in (("sequential", "Single-threaded (sequential) server", "s-"),
                              ("mt", "Multi-threaded server", "o-")):
            pts = sorted((n, v) for (t, m, n), v in sum1.items() if t == topic and m == mode)
            if not pts:
                continue
            xs = [p[0] for p in pts]
            ys = [p[1]["mean"] / MB for p in pts]
            es = [p[1]["sd"] / MB for p in pts]
            ax.errorbar(xs, ys, yerr=es, fmt=mk, capsize=4, label=lab, lw=2)
            for p in pts:
                pdata.append([mode, p[0], p[1]["mean"] / MB, p[1]["sd"] / MB, p[1]["n"]])
        ax.set_xscale("log", base=2)
        ax.set_xticks([1, 2, 4, 8])
        ax.set_xticklabels(["1", "2", "4", "8"])
        ax.set_xlabel("Number of concurrent clients (single host, loopback)")
        ax.set_ylabel("Aggregate useful throughput (MB/s, 1 MB = 10^6 B)")
        ax.set_title(f"Part I: aggregate file-transfer rate vs. clients\n(topic '{topic}', real Animals-10 images; mean ± 1 SD)")
        ax.set_ylim(bottom=0)
        ax.legend()
        base = "Part1_throughput" if topic == topics[0] else f"Part1_throughput_{topic}"
        save(fig, base)
        write(f"plotdata_{base}.csv", ["server_mode", "clients", "mean_MBps", "sd_MBps", "n"], pdata)

# ============================================================ Experiments 2 & 3
def fault_summary(rows):
    g = defaultdict(list)
    for r in measured(rows):
        g[(r["implementation"], r["topic"], r["failure_probability"])].append(r)
    out, res = [], {}
    for key, rs in sorted(g.items()):
        ok = [r for r in rs if r["integrity_pass"] == "1"]
        col = lambda name: [fl(r[name]) for r in ok]
        thr, el = stats(col("useful_throughput_bytes_per_sec")), stats(col("elapsed_seconds"))
        ovh, tx, use = stats(col("byte_overhead_pct")), stats(col("total_transmitted_bytes")), stats(col("useful_bytes"))
        red, ctl = stats(col("redundant_payload_bytes")), stats(col("protocol_control_bytes"))
        rc, fa = stats(col("reconnects")), stats(col("faults_injected_server"))
        res[key] = dict(thr=thr, el=el, ovh=ovh, tx=tx, use=use, red=red, ctl=ctl, rc=rc, fa=fa, n=len(ok), total=len(rs))
        out.append([key[0], key[1], key[2], len(rs), len(ok), len(rs) - len(ok),
                    fmt(thr[0], 1), fmt(thr[1], 1), fmt(thr[2], 1), fmt(el[0]), fmt(el[1]), fmt(el[2]),
                    fmt(use[0], 1), fmt(tx[0], 1), fmt(tx[2], 1), fmt(red[0], 1), fmt(red[2], 1),
                    fmt(ctl[0], 1), fmt(ovh[0], 4), fmt(ovh[1], 4), fmt(ovh[2], 4), fmt(rc[0], 3), fmt(fa[0], 3)])
    return out, res


FH = ["implementation", "topic", "failure_probability", "measured_runs", "successful_runs", "failed_runs",
      "mean_useful_throughput_Bps", "median_useful_throughput_Bps", "sd_useful_throughput_Bps",
      "mean_completion_s", "median_completion_s", "sd_completion_s", "mean_useful_bytes",
      "mean_total_transmitted_bytes", "sd_total_transmitted_bytes", "mean_redundant_payload_bytes",
      "sd_redundant_payload_bytes", "mean_protocol_control_bytes", "mean_byte_overhead_pct",
      "median_byte_overhead_pct", "sd_byte_overhead_pct", "mean_reconnects", "mean_faults_injected"]
exp2, exp3 = read("exp2_case1_raw.csv"), read("exp3_case2_raw.csv")
s2 = s3 = {}
if exp2:
    o, s2 = fault_summary(exp2)
    write("exp2_case1_summary.csv", FH, o)
if exp3:
    o, s3 = fault_summary(exp3)
    write("exp3_case2_summary.csv", FH, o)

TOPIC_CMP = "gatto_fault20"
if s2 and s3:
    probs = sorted({k[2] for k in list(s2) + list(s3) if k[1] == TOPIC_CMP and (("Case1", TOPIC_CMP, k[2]) in s2) and (("Case2", TOPIC_CMP, k[2]) in s3)}, key=float)
    rows = []
    for p in probs:
        a, b = s2[("Case1", TOPIC_CMP, p)], s3[("Case2", TOPIC_CMP, p)]
        for lab, s in (("Case1", a), ("Case2", b)):
            rows.append([TOPIC_CMP, p, lab, s["total"], s["n"], fmt(s["thr"][0], 1), fmt(s["thr"][2], 1),
                         fmt(s["el"][0]), fmt(s["el"][2]), fmt(s["use"][0], 1), fmt(s["tx"][0], 1),
                         fmt(s["red"][0], 1), fmt(s["ctl"][0], 1), fmt(s["ovh"][0], 4), fmt(s["ovh"][2], 4),
                         fmt(s["rc"][0], 3), fmt(s["fa"][0], 3)])
    write("exp23_case_comparison.csv",
          ["topic", "failure_probability", "implementation", "measured_runs", "successful_runs",
           "mean_useful_throughput_Bps", "sd_useful_throughput_Bps", "mean_completion_s", "sd_completion_s",
           "mean_useful_bytes", "mean_total_transmitted_bytes", "mean_redundant_payload_bytes",
           "mean_protocol_control_bytes", "mean_byte_overhead_pct", "sd_byte_overhead_pct",
           "mean_reconnects", "mean_faults_injected"], rows)

    def two_series(metric, scale, ylabel, title, base, logy=False, kind="line"):
        fig, ax = plt.subplots(figsize=(7.5, 5))
        xs = list(range(len(probs)))
        pdata = []
        for lab, sm, mk, off in (("Case 1 (no session mgmt, restart from beginning)", s2, "s-", -0.04),
                                 ("Case 2 (session mgmt, checkpoint resume)", s3, "o-", 0.04)):
            key = "Case1" if sm is s2 else "Case2"
            ys = [sm[(key, TOPIC_CMP, p)][metric][0] / scale for p in probs]
            es = [sm[(key, TOPIC_CMP, p)][metric][2] / scale for p in probs]
            ax.errorbar([x + off for x in xs], ys, yerr=es, fmt=mk, capsize=4, lw=2, label=lab)
            for p, y, e in zip(probs, ys, es):
                pdata.append([key, p, y, e, sm[(key, TOPIC_CMP, p)]["n"]])
        ax.set_xticks(xs)
        ax.set_xticklabels(probs)
        ax.set_xlabel("Server failure probability (per 64 KiB chunk)")
        ax.set_ylabel(ylabel)
        ax.set_title(title)
        if logy:
            ax.set_yscale("log")
        else:
            ax.set_ylim(bottom=0)
        ax.legend(fontsize=9)
        save(fig, base)
        write(f"plotdata_{base}.csv", ["implementation", "failure_probability", "mean", "sd", "n_successful"], pdata)

    sub = f"topic '{TOPIC_CMP}' (20 chunks, real Animals-10 images); mean ± 1 SD"
    two_series("thr", MB, "Useful throughput (MB/s)", f"Case 1 vs Case 2: useful throughput\n{sub}", "Case1_Case2_throughput")
    two_series("ovh", 1.0, "Byte overhead (%)  = (total − useful) / useful × 100",
               f"Case 1 vs Case 2: byte overhead\n{sub}", "Case1_Case2_overhead")
    two_series("el", 1.0, "Completion time (s, log scale)", f"Case 1 vs Case 2: completion time\n{sub}",
               "Case1_Case2_completion_time", logy=True)
    two_series("red", 1e3, "Redundant payload bytes (kB)", f"Case 1 vs Case 2: redundant payload\n{sub}",
               "Case1_Case2_redundant_bytes")

# ============================================================ Experiment 4
exp4 = read("exp4_enhanced_raw.csv")
if exp4:
    g = defaultdict(list)
    for r in measured(exp4):
        lab = "default" if r["cli_form"].startswith("default") else r["num_connections"]
        g[(r["topic"], r["failure_probability"], str(lab))].append(r)
    res, rows = {}, []
    for key, rs in sorted(g.items()):
        ok = [r for r in rs if r["integrity_pass"] == "1"]
        col = lambda name: [fl(r[name]) for r in ok]
        res[key] = dict(thr=stats(col("useful_throughput_bytes_per_sec")), el=stats(col("elapsed_seconds")),
                        ovh=stats(col("byte_overhead_pct")), tx=stats(col("total_transmitted_bytes")),
                        red=stats(col("redundant_payload_bytes")), ctl=stats(col("protocol_control_bytes")),
                        rc=stats(col("reconnects")), fa=stats(col("faults_injected_server")),
                        use=stats(col("useful_bytes")), n=len(ok), total=len(rs))
    for key, s in sorted(res.items()):
        topic, p, n = key
        base = res.get((topic, p, "1"))
        sp = spsd = float("nan")
        if base and n != "default" and base["thr"][0] == base["thr"][0] and base["thr"][0] > 0:
            sp = s["thr"][0] / base["thr"][0]
            rel = math.sqrt((s["thr"][2] / s["thr"][0]) ** 2 + (base["thr"][2] / base["thr"][0]) ** 2) if s["thr"][0] else 0
            spsd = sp * rel
        rows.append([topic, p, n, s["total"], s["n"], s["total"] - s["n"],
                     fmt(s["thr"][0], 1), fmt(s["thr"][1], 1), fmt(s["thr"][2], 1),
                     fmt(s["el"][0]), fmt(s["el"][1]), fmt(s["el"][2]),
                     fmt(sp, 4), fmt(spsd, 4), fmt(s["use"][0], 1), fmt(s["tx"][0], 1),
                     fmt(s["red"][0], 1), fmt(s["ctl"][0], 1), fmt(s["ovh"][0], 4), fmt(s["ovh"][2], 4),
                     fmt(s["rc"][0], 3), fmt(s["fa"][0], 3)])
    write("exp4_enhanced_summary.csv",
          ["topic", "failure_probability", "num_connections", "measured_runs", "successful_runs", "failed_runs",
           "mean_useful_throughput_Bps", "median_useful_throughput_Bps", "sd_useful_throughput_Bps",
           "mean_completion_s", "median_completion_s", "sd_completion_s",
           "speedup_vs_N1_same_topic_and_p", "speedup_error_propagated_sd", "mean_useful_bytes",
           "mean_total_transmitted_bytes", "mean_redundant_payload_bytes", "mean_protocol_control_bytes",
           "mean_byte_overhead_pct", "sd_byte_overhead_pct", "mean_reconnects", "mean_faults_injected"], rows)

    def enh_plot(metric, scale, ylabel, title, base, speedup=False):
        fig, ax = plt.subplots(figsize=(7.8, 5.2))
        pdata = []
        styles = {}
        for (topic, p, n), s in sorted(res.items()):
            pass
        series = sorted({(t, p) for (t, p, n) in res})
        for i, (topic, p) in enumerate(series):
            ns = sorted([int(n) for (t, pp, n) in res if t == topic and pp == p and n != "default"])
            if not ns:
                continue
            ys, es = [], []
            for n in ns:
                s = res[(topic, p, str(n))]
                b = res.get((topic, p, "1"))
                if speedup:
                    y = s["thr"][0] / b["thr"][0] if b and b["thr"][0] else float("nan")
                    rel = math.sqrt((s["thr"][2] / s["thr"][0]) ** 2 + (b["thr"][2] / b["thr"][0]) ** 2) if s["thr"][0] else 0
                    ys.append(y); es.append(y * rel)
                else:
                    ys.append(s[metric][0] / scale); es.append(s[metric][2] / scale)
                pdata.append([topic, p, n, ys[-1], es[-1], s["n"]])
            ax.errorbar(ns, ys, yerr=es, fmt=["o-", "s--", "^-", "d--"][i % 4], capsize=4, lw=2,
                        label=f"{topic}, p={p}")
        if speedup:
            ax.plot([1, 2, 4], [1, 2, 4], ":", color="gray", label="ideal linear speedup (reference)")
            ax.axhline(1.0, color="k", lw=0.6)
        ax.set_xscale("log", base=2)
        ax.set_xticks([1, 2, 4]); ax.set_xticklabels(["1", "2", "4"])
        ax.set_xlabel("Parallel TCP connections N")
        ax.set_ylabel(ylabel)
        ax.set_title(title)
        if not speedup:
            ax.set_ylim(bottom=0)
        ax.legend(fontsize=8)
        save(fig, base)
        write(f"plotdata_{base}.csv", ["topic", "failure_probability", "N", "value", "sd", "n_successful"], pdata)

    enh_plot("thr", MB, "Useful throughput (MB/s)", "Case 2 Enhanced: useful throughput vs. connections\n(mean ± 1 SD)", "Case2_Enhanced_throughput")
    enh_plot("thr", MB, "Speedup = throughput(N) / throughput(1)", "Case 2 Enhanced: speedup vs. 1 connection\n(same topic and failure probability; mean, error bars propagated)", "Case2_Enhanced_speedup", speedup=True)
    enh_plot("el", 1.0, "Completion time (s)", "Case 2 Enhanced: completion time vs. connections\n(mean ± 1 SD)", "Case2_Enhanced_completion_time")
print("analysis done")
