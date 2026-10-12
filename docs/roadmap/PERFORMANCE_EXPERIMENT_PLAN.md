# PERFORMANCE EXPERIMENT PLAN & METRIC SPECIFICATION
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Document**: Performance Measurement Methodology, Metrics, CSV Schema & Graph Specs  
**Target Environment**: 3-Node Lab Network / Dedicated Loopback  

---

## 1. Metric Definitions & Formulas

To ensure rigorous, scientifically sound evaluation without ambiguity, all metrics are formally defined:

### 1.1 Time Boundaries
Elapsed time is measured using monotonic clocks (`clock_gettime(CLOCK_MONOTONIC)`) to eliminate distortion from NTP adjustments or wall-clock shifts:
$$\Delta T = T_{\text{end}} - T_{\text{start}}$$
- **$T_{\text{start}}$**: Recorded by the client immediately before transmitting the first byte of `MSG_GET`.
- **$T_{\text{end}}$**: Recorded by the client immediately after receiving `MSG_CLOSE`, validating all files on disk, and before socket teardown.

### 1.2 Byte Counters
- **Useful Bytes ($B_{\text{useful}}$)**: The sum of file sizes of all unique files successfully transferred and committed to disk.
  $$B_{\text{useful}} = \sum_{i=1}^{N} \text{filesize}(f_i)$$
- **Socket Bytes ($B_{\text{socket}}$)**: The total cumulative bytes passed to `send()` by the server or read from `recv()` by the client, including all 16-byte protocol headers, control frames, and payload bytes.
- **Redundant Bytes ($B_{\text{redundant}}$)**: File payload bytes retransmitted after a connection interruption that overwrite or duplicate previously delivered data.
  - In **Case 1**: Since the entire transfer restarts, any byte transferred prior to an injected failure is classified as redundant.
  - In **Case 2**: Since transfer resumes from the last acknowledged chunk offset, redundant bytes are bounded to at most unacknowledged in-flight chunks.
- **Retransmission Overhead ($O_{\text{retransmit}}$)**:
  $$O_{\text{retransmit}} = \frac{B_{\text{redundant}}}{B_{\text{useful}}} \times 100\%$$
- **Protocol Overhead ($O_{\text{protocol}}$)**:
  $$O_{\text{protocol}} = \frac{B_{\text{socket}} - B_{\text{useful}}}{B_{\text{useful}}} \times 100\%$$

### 1.3 Throughput & Transfer Rates
- **Per-Client Throughput ($R_{\text{client}}$)**:
  $$R_{\text{client}} = \frac{B_{\text{useful}}}{\Delta T} \quad \text{[MB/sec]}$$
- **Aggregate Transfer Rate ($R_{\text{aggregate}}$)** for concurrent clients ($C$):
  $$R_{\text{aggregate}} = \frac{\sum_{k=1}^{C} B_{\text{useful}}^{(k)}}{\max(T_{\text{end}}^{(k)}) - \min(T_{\text{start}}^{(k)})} \quad \text{[MB/sec]}$$

---

## 2. Experimental Matrices

---

### Experiment 1: Part 1 Sequential vs. Multi-Threaded Scalability
- **Goal**: Evaluate the performance impact of multi-threading as concurrent client load increases.
- **Independent Variables**:
  - Server Mode: Single-Threaded vs. Multi-Threaded
  - Concurrency ($C$): 1, 2, 4, and 8 concurrent clients
  - Dataset Topic: Fixed topic (e.g. `cane`, ~50MB of images)
- **Measured Outputs**: Aggregate file transfer rate (MB/s), total completion time (sec).
- **Hypothesis**: Sequential server throughput drops or plateaus due to head-of-line blocking; multi-threaded server maintains higher aggregate rate by serving clients concurrently.

---

### Experiment 2: Part 2 Case 1 vs. Case 2 Fault Resilience
- **Goal**: Quantify the performance advantage of session management and checkpoint resume under increasing failure rates.
- **Independent Variables**:
  - Recovery Strategy: Case 1 (Full Restart) vs. Case 2 (Checkpoint Resume)
  - Failure Probability ($p$): 0.0 (0%), 0.01 (1%), 0.05 (5%), 0.10 (10%), 0.20 (20%)
  - Concurrency: 1 client and 4 clients
- **Measured Outputs**: Total completion time (sec), redundant bytes retransmitted (MB), throughput (MB/s).
- **Hypothesis**: In Case 1, completion time and redundant bytes grow exponentially with failure rate; in Case 2, redundant bytes remain near zero, and completion time increases only linearly with backoff reconnect latency.

---

### Experiment 3: Case 2 Enhanced Parallel TCP Scaling
- **Goal**: Measure speedup achieved by striping file transfers across multiple simultaneous TCP connections.
- **Independent Variables**:
  - Connection Count ($K$): 1 (Baseline Case 2), 2, 4, 8 connections
  - Topic Size: Multi-file topic (`cavallo`, 100+ images) and Single large file (50MB)
  - Failure Probability: 0.0 and 0.05
- **Measured Outputs**: Total transfer time (sec), aggregate throughput (MB/s), speedup factor $S = T_1 / T_K$.
- **Hypothesis**: Parallel connections increase link utilization and reduce transfer time up to the network/disk saturation knee.

---

## 3. Raw Data CSV Schema

All raw benchmark data will be outputted to files in `Results/`:
- `Results/part1_benchmark.csv`
- `Results/part2_case1_vs_case2.csv`
- `Results/part2_enhanced_scaling.csv`

### 3.1 CSV Columns Specification

```csv
case_name,concurrency,failure_prob,parallel_conns,topic,total_useful_bytes,total_socket_bytes,redundant_bytes,elapsed_sec,throughput_mbps,reconnect_count,status
Part1_Seq,1,0.0,1,cane,52428800,52435000,0,5.21,10.06,0,SUCCESS
Part1_MT,4,0.0,1,cane,209715200,209740000,0,8.45,24.81,0,SUCCESS
Part2_Case1,1,0.05,1,cane,52428800,78643200,26214400,12.30,4.26,3,SUCCESS
Part2_Case2,1,0.05,1,cane,52428800,52800000,16384,6.10,8.59,3,SUCCESS
Part2_Enhanced,1,0.0,4,cane,52428800,52500000,0,2.15,24.38,0,SUCCESS
```

---

## 4. Required Graphs & Visualizations

The script `scripts/plot_results.py` will generate the following publication-quality graphs:

1. **`Results/Part1_throughput_vs_clients.png`**:
   - X-axis: Number of Concurrent Clients (1, 2, 4, 8)
   - Y-axis: Aggregate File Transfer Rate (MB/s)
   - Series: Sequential Server vs Multi-Threaded Server
2. **`Results/Part2_Case1_vs_Case2_throughput.png`**:
   - X-axis: Fault Injection Probability (0%, 1%, 5%, 10%, 20%)
   - Y-axis: Effective Throughput (MB/s)
   - Series: Case 1 (Restart) vs Case 2 (Resume)
3. **`Results/Part2_Case1_vs_Case2_overhead.png`**:
   - X-axis: Fault Injection Probability (0%, 1%, 5%, 10%, 20%)
   - Y-axis: Redundant Retransmitted Bytes (MB)
   - Series: Case 1 vs Case 2 (Bar chart with log scale)
4. **`Results/Part2_Case2_vs_Enhanced_speedup.png`**:
   - X-axis: Number of Parallel TCP Connections (1, 2, 4, 8)
   - Y-axis: Speedup Factor ($T_1 / T_K$)
   - Series: Measured Speedup vs Ideal Linear Speedup

---

## 5. Guidelines for Analysis Section in Final Report

1. **Explain the Crossover Points**: Why does multi-threading improve throughput up to a certain client count before diminishing returns set in? (Context switching, socket buffer contention, disk seek contention).
2. **Quantify the Recovery Gain**: Calculate the exact percentage of redundant data saved by Case 2 over Case 1:
   $$\text{Savings} = \left( 1 - \frac{B_{\text{redundant}}^{\text{Case 2}}}{B_{\text{redundant}}^{\text{Case 1}}} \right) \times 100\%$$
3. **Analyze Enhanced Parallelism Bottlenecks**: Document when parallel connections help (many independent medium files, latency-bound links) versus when they offer limited benefit (disk I/O bottlenecks).
4. **Academic Integrity Rule**: Never invent experimental numbers. If a test shows overhead or slower speed, report it accurately and analyze the systems-level reason.
