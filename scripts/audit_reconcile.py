#!/usr/bin/env python3
import csv, math

def main():
    print("=== AUDIT VERIFICATION OF ALL BENCHMARK DATA ===")
    
    # 1. Manifest verification
    with open('Results/benchmark_run_manifest.csv') as f:
        manifest = list(csv.DictReader(f))
    print(f"Total manifest rows: {len(manifest)}")
    assert len(manifest) == 176, f"Expected 176 manifest rows, got {len(manifest)}"
    run_ids = set(r['run_id'] for r in manifest)
    assert len(run_ids) == 176, "Duplicate run IDs in manifest!"
    for r in manifest:
        assert r['integrity_outcome'] == 'PASS', f"Integrity not PASS: {r['run_id']}"
    print("Manifest: 176/176 runs unique, 100% PASS.")

    # 2. Exp 1
    with open('Results/exp1_part1_raw.csv') as f:
        r1 = list(csv.DictReader(f))
    with open('Results/exp1_part1_summary.csv') as f:
        s1 = list(csv.DictReader(f))
    assert len(r1) == 32
    for s in s1:
        mode = s['server_mode']
        c = int(s['client_count'])
        matches = [r for r in r1 if r['server_mode'] == mode and int(r['client_count']) == c and r['is_warmup'] == '0']
        assert len(matches) == int(s['measured_runs']) == 3
        thr = sum(float(r['aggregate_useful_throughput_bytes_per_sec']) for r in matches) / 3
        assert abs(thr - float(s['mean_agg_useful_throughput_Bps'])) < 0.2
    print("Exp 1: 32 raw rows (8 warm-up + 24 measured), summary perfectly verified.")

    # 3. Exp 2
    with open('Results/exp2_case1_raw.csv') as f:
        r2 = list(csv.DictReader(f))
    with open('Results/exp2_case1_summary.csv') as f:
        s2 = list(csv.DictReader(f))
    assert len(r2) == 24
    for s in s2:
        p = float(s['failure_probability'])
        matches = [r for r in r2 if float(r['failure_probability']) == p and r['is_warmup'] == '0']
        assert len(matches) == int(s['measured_runs']) == 5
        mean_time = sum(float(r['elapsed_seconds']) for r in matches) / 5
        assert abs(mean_time - float(s['mean_completion_s'])) < 0.001
    print("Exp 2: 24 raw rows (4 warm-up + 20 measured), summary perfectly verified.")

    # 4. Exp 3
    with open('Results/exp3_case2_raw.csv') as f:
        r3 = list(csv.DictReader(f))
    with open('Results/exp3_case2_summary.csv') as f:
        s3 = list(csv.DictReader(f))
    assert len(r3) == 36
    for s in s3:
        p = float(s['failure_probability'])
        top = s['topic']
        matches = [r for r in r3 if r['topic'] == top and float(r['failure_probability']) == p and r['is_warmup'] == '0']
        assert len(matches) == int(s['measured_runs']) == 5
        mean_time = sum(float(r['elapsed_seconds']) for r in matches) / 5
        assert abs(mean_time - float(s['mean_completion_s'])) < 0.001
    print("Exp 3: 36 raw rows (6 warm-up + 30 measured), summary perfectly verified.")

    # 5. Exp 4
    with open('Results/exp4_enhanced_raw.csv') as f:
        r4 = list(csv.DictReader(f))
    with open('Results/exp4_enhanced_summary.csv') as f:
        s4 = list(csv.DictReader(f))
    assert len(r4) == 84
    for s in s4:
        p = float(s['failure_probability'])
        top = s['topic']
        conn = s['num_connections']
        def is_m(r):
            if r['topic'] != top or float(r['failure_probability']) != p or r['is_warmup'] != '0':
                return False
            lab = 'default' if r['cli_form'].startswith('default') else r['num_connections']
            return lab == conn
        matches = [r for r in r4 if is_m(r)]
        assert len(matches) == int(s['measured_runs']) == 5
        mean_time = sum(float(r['elapsed_seconds']) for r in matches) / 5
        assert abs(mean_time - float(s['mean_completion_s'])) < 0.001
    print("Exp 4: 84 raw rows (14 warm-up + 70 measured), summary perfectly verified.")

    print(f"Total raw runs: {len(r1) + len(r2) + len(r3) + len(r4)} == 176!")
    print("ALL 176 RUNS AND SUMMARIES ARE 100% MATHEMATICALLY VERIFIED!")

if __name__ == '__main__':
    main()
