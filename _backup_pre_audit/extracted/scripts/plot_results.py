#!/usr/bin/env python3
"""
==============================================================================
IT305 Computer Networks Lab - Autumn 2026
Project: Fault-Tolerant Topic-Based File Distribution Service

Script: scripts/plot_results.py
Description: Publication-Quality Matplotlib Graph Generator for Results/*.csv
Phase: Phase 6 (Sole Owner: Yug Malaviya - 202401256)

Generates the 4 required PNG graphs in Results/:
  1. Results/Part1_throughput_vs_clients.png
  2. Results/Part2_Case1_vs_Case2_throughput.png
  3. Results/Part2_Case1_vs_Case2_overhead.png
  4. Results/Part2_Enhanced_speedup_comparison.png
==============================================================================
"""

from pathlib import Path
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT_DIR = Path(__file__).resolve().parent.parent
RESULTS_DIR = ROOT_DIR / "Results"

plt.rcParams.update({
    "font.family": "sans-serif",
    "font.sans-serif": ["DejaVu Sans", "Arial", "Helvetica"],
    "font.size": 10.5,
    "axes.titlesize": 12.5,
    "axes.titleweight": "bold",
    "axes.labelsize": 11,
    "axes.labelweight": "bold",
    "xtick.labelsize": 10,
    "ytick.labelsize": 10,
    "legend.fontsize": 10,
    "figure.dpi": 200,
    "savefig.dpi": 200,
    "axes.grid": True,
    "grid.alpha": 0.28,
    "grid.linestyle": "--",
})

NAVY = "#1B365D"
TEAL = "#008080"
CORAL = "#D9534F"


def plot_exp1():
    df = pd.read_csv(RESULTS_DIR / "exp1_part1_concurrency.csv")
    grouped = (
        df.groupby(["server_mode", "num_clients"])["throughput_mbps"]
        .agg(["mean", "median", "std", "count"])
        .reset_index()
    )
    grouped["sem"] = grouped["std"] / np.sqrt(grouped["count"])

    fig, ax = plt.subplots(figsize=(7.6, 4.6))
    x_pos = np.arange(4)
    clients_order = [1, 2, 4, 8]

    for mode, color, marker, offset_y in [
        ("Sequential", CORAL, "o", -16),
        ("Multi-Threaded", NAVY, "s", 10),
    ]:
        sub = grouped[grouped["server_mode"] == mode].set_index("num_clients").loc[clients_order]
        ax.errorbar(
            x_pos,
            sub["mean"],
            yerr=sub["sem"],
            label=f"Part 1 {mode} Server",
            color=color,
            marker=marker,
            markersize=7,
            linewidth=2.3,
            capsize=4,
        )
        for xi, (_, row) in zip(x_pos, sub.iterrows()):
            ax.annotate(
                f"{row['mean']:.1f} MB/s",
                (xi, row["mean"]),
                textcoords="offset points",
                xytext=(0, offset_y),
                ha="center",
                fontsize=8.5,
                color=color,
                fontweight="bold",
            )

    ax.set_title("Experiment 1: Part 1 Aggregate Throughput vs. Concurrent Clients")
    ax.set_xlabel("Number of Concurrent Clients (C)")
    ax.set_ylabel("Aggregate Transfer Rate (MB/s)")
    ax.set_xticks(x_pos)
    ax.set_xticklabels(["1 Client", "2 Clients", "4 Clients", "8 Clients"])
    top_val = (grouped["mean"] + grouped["sem"]).max()
    ax.set_ylim(bottom=0, top=top_val * 1.25)
    ax.legend(loc="upper left", frameon=True)
    fig.tight_layout()
    out_path = RESULTS_DIR / "Part1_throughput_vs_clients.png"
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)
    print(f"[PLOT] Generated {out_path}")


def plot_exp2():
    df = pd.read_csv(RESULTS_DIR / "exp2_fault_tolerance.csv")
    grouped = (
        df.groupby(["case_name", "failure_prob"])["throughput_mbps"]
        .agg(["mean", "median", "std", "count"])
        .reset_index()
    )
    grouped["sem"] = grouped["std"] / np.sqrt(grouped["count"])

    fig, ax = plt.subplots(figsize=(7.6, 4.6))
    x_pos = np.arange(4)
    probs_order = [0.00, 0.01, 0.05, 0.10]

    for case_name, label, color, marker, offset_y in [
        ("Case1", "Part 2 Case 1 (Stateless Restart)", CORAL, "o", -16),
        ("Case2", "Part 2 Case 2 (Stateful Checkpoint Resume)", TEAL, "s", 10),
    ]:
        sub = grouped[grouped["case_name"] == case_name].set_index("failure_prob").loc[probs_order]
        ax.errorbar(
            x_pos,
            sub["mean"],
            yerr=sub["sem"],
            label=label,
            color=color,
            marker=marker,
            markersize=7,
            linewidth=2.3,
            capsize=4,
        )
        for xi, (_, row) in zip(x_pos, sub.iterrows()):
            ax.annotate(
                f"{row['mean']:.1f} MB/s",
                (xi, row["mean"]),
                textcoords="offset points",
                xytext=(0, offset_y),
                ha="center",
                fontsize=8.5,
                color=color,
                fontweight="bold",
            )

    ax.set_title("Experiment 2: Effective Throughput vs. Fault Injection Probability")
    ax.set_xlabel("Chunk Failure Probability p")
    ax.set_ylabel("Effective Useful Throughput (MB/s)")
    ax.set_xticks(x_pos)
    ax.set_xticklabels(["0% (p = 0.00)", "1% (p = 0.01)", "5% (p = 0.05)", "10% (p = 0.10)"])
    top_val = (grouped["mean"] + grouped["sem"]).max()
    ax.set_ylim(bottom=0, top=top_val * 1.28)
    ax.legend(loc="upper right", frameon=True)
    fig.tight_layout()
    out_path = RESULTS_DIR / "Part2_Case1_vs_Case2_throughput.png"
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)
    print(f"[PLOT] Generated {out_path}")


def plot_exp3():
    df = pd.read_csv(RESULTS_DIR / "exp3_overhead_comparison.csv")
    grouped = (
        df.groupby(["case_name", "failure_prob"])["overhead_ratio_percent"]
        .agg(["mean", "median", "std", "count"])
        .reset_index()
    )
    grouped["sem"] = grouped["std"] / np.sqrt(grouped["count"])

    probs = [0.00, 0.01, 0.05, 0.10]
    x = np.arange(len(probs))
    width = 0.35

    fig, ax = plt.subplots(figsize=(7.6, 4.6))

    c1 = grouped[grouped["case_name"] == "Case1"].set_index("failure_prob").loc[probs]
    c2 = grouped[grouped["case_name"] == "Case2"].set_index("failure_prob").loc[probs]

    rects1 = ax.bar(
        x - width / 2,
        c1["mean"],
        width,
        yerr=c1["sem"],
        capsize=4,
        label="Part 2 Case 1 (Full Topic Restart)",
        color=CORAL,
        edgecolor="#222222",
        linewidth=0.7,
    )
    rects2 = ax.bar(
        x + width / 2,
        c2["mean"],
        width,
        yerr=c2["sem"],
        capsize=4,
        label="Part 2 Case 2 (Session Checkpoint Resume)",
        color=NAVY,
        edgecolor="#222222",
        linewidth=0.7,
    )

    for rect, sem_v in zip(rects1, c1["sem"]):
        h = rect.get_height()
        ax.annotate(
            f"{h:.1f}%",
            xy=(rect.get_x() + rect.get_width() / 2, h + sem_v),
            xytext=(0, 5),
            textcoords="offset points",
            ha="center",
            fontsize=8.5,
            fontweight="bold",
            color=CORAL,
        )

    for rect, sem_v in zip(rects2, c2["sem"]):
        h = rect.get_height()
        ax.annotate(
            f"{h:.2f}%",
            xy=(rect.get_x() + rect.get_width() / 2, h + sem_v),
            xytext=(0, 5),
            textcoords="offset points",
            ha="center",
            fontsize=8.5,
            fontweight="bold",
            color=NAVY,
        )

    ax.set_title("Experiment 3: Protocol & Retransmission Overhead vs. Failure Rate")
    ax.set_xlabel("Chunk Failure Probability p")
    ax.set_ylabel("Overhead Ratio ((B_total - B_useful) / B_useful) [%]")
    ax.set_xticks(x)
    ax.set_xticklabels(["0% (p = 0.00)", "1% (p = 0.01)", "5% (p = 0.05)", "10% (p = 0.10)"])
    top_val = max((c1["mean"] + c1["sem"]).max(), (c2["mean"] + c2["sem"]).max())
    ax.set_ylim(bottom=0, top=top_val * 1.25 + 4)
    ax.legend(loc="upper left", frameon=True)
    fig.tight_layout()
    out_path = RESULTS_DIR / "Part2_Case1_vs_Case2_overhead.png"
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)
    print(f"[PLOT] Generated {out_path}")


def plot_exp4():
    df = pd.read_csv(RESULTS_DIR / "exp4_enhanced_speedup.csv")
    grouped = (
        df.groupby(["failure_prob", "num_connections"])
        .agg(
            tp_mean=("throughput_mbps", "mean"),
            tp_std=("throughput_mbps", "std"),
            sp_mean=("speedup_factor", "mean"),
            sp_std=("speedup_factor", "std"),
            cnt=("throughput_mbps", "count"),
        )
        .reset_index()
    )
    grouped["tp_sem"] = grouped["tp_std"] / np.sqrt(grouped["cnt"])
    grouped["sp_sem"] = grouped["sp_std"] / np.sqrt(grouped["cnt"])

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10.6, 4.5))
    x_pos = np.arange(3)
    conns_order = [1, 2, 4]

    for prob, label, color, marker, offset_y in [
        (0.00, "No Faults (p = 0.00)", NAVY, "o", 10),
        (0.05, "With Faults (p = 0.05)", TEAL, "s", -16),
    ]:
        sub = (
            grouped[np.isclose(grouped["failure_prob"], prob)]
            .set_index("num_connections")
            .loc[conns_order]
        )
        ax1.errorbar(
            x_pos,
            sub["tp_mean"],
            yerr=sub["tp_sem"],
            label=label,
            color=color,
            marker=marker,
            markersize=7,
            linewidth=2.3,
            capsize=4,
        )
        for xi, (_, row) in zip(x_pos, sub.iterrows()):
            ax1.annotate(
                f"{row['tp_mean']:.1f}",
                (xi, row["tp_mean"]),
                textcoords="offset points",
                xytext=(0, offset_y),
                ha="center",
                fontsize=8.5,
                color=color,
                fontweight="bold",
            )

        ax2.errorbar(
            x_pos,
            sub["sp_mean"],
            yerr=sub["sp_sem"],
            label=label,
            color=color,
            marker=marker,
            markersize=7,
            linewidth=2.3,
            capsize=4,
        )
        for xi, (_, row) in zip(x_pos, sub.iterrows()):
            ax2.annotate(
                f"{row['sp_mean']:.2f}x",
                (xi, row["sp_mean"]),
                textcoords="offset points",
                xytext=(0, offset_y),
                ha="center",
                fontsize=8.5,
                color=color,
                fontweight="bold",
            )

    ax1.set_title("Enhanced Throughput vs. Parallel Streams")
    ax1.set_xlabel("Parallel TCP Connections (N)")
    ax1.set_ylabel("Throughput (MB/s)")
    ax1.set_xticks(x_pos)
    ax1.set_xticklabels(["N = 1 Stream", "N = 2 Streams", "N = 4 Streams"])
    ax1.set_ylim(bottom=0, top=(grouped["tp_mean"] + grouped["tp_sem"]).max() * 1.25)
    ax1.legend(loc="upper left", frameon=True)

    ax2.set_title("Speedup Factor Relative to N=1 Stream")
    ax2.set_xlabel("Parallel TCP Connections (N)")
    ax2.set_ylabel("Speedup Factor (T_1 / T_N)")
    ax2.set_xticks(x_pos)
    ax2.set_xticklabels(["N = 1 Stream", "N = 2 Streams", "N = 4 Streams"])
    ax2.set_ylim(bottom=0.6, top=(grouped["sp_mean"] + grouped["sp_sem"]).max() * 1.25)
    ax2.legend(loc="upper left", frameon=True)

    fig.suptitle("Experiment 4: Part 2 Case 2 Enhanced Parallel TCP Scaling", fontsize=13, fontweight="bold", y=1.02)
    fig.tight_layout()
    out_path = RESULTS_DIR / "Part2_Enhanced_speedup_comparison.png"
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)
    print(f"[PLOT] Generated {out_path}")


def main():
    plot_exp1()
    plot_exp2()
    plot_exp3()
    plot_exp4()
    print("[PLOT] All 4 publication-grade plots generated successfully!")


if __name__ == "__main__":
    main()
