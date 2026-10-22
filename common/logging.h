/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/logging.h
 * Description: Thread-Safe Monotonic Logging and Benchmark CSV Instrumentation.
 *
 * Phase: Phase 6 (Logging, Automated Test Suite, Benchmarks, Report & Packaging)
 * Owner: Yug Malaviya (202401256)
 * Status: FROZEN v1.0
 * ============================================================================
 */

#ifndef LOGGING_H
#define LOGGING_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
} LogLevel;

typedef struct {
    uint32_t run_id;
    char case_name[64];
    char topic[128];
    uint32_t num_clients;
    uint32_t num_connections;
    double failure_prob;
    uint64_t useful_bytes;
    uint64_t redundant_bytes;
    uint64_t total_socket_bytes;
    uint64_t start_monotonic_ns;
    uint64_t end_monotonic_ns;
    double elapsed_sec;
    double throughput_mbps;
    double overhead_ratio_percent;
    uint32_t retry_count;
} BenchmarkRecord;

uint64_t get_monotonic_time_ns(void);
void log_event(LogLevel level, const char *module, const char *fmt, ...);
int record_benchmark_metric(const BenchmarkRecord *record, const char *csv_path);
void set_min_log_level(LogLevel min_level);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_H */
