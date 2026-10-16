/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/logging.c
 * Description: Implementation of Thread-Safe Monotonic Logger & CSV Recorder.
 *
 * Phase: Phase 6 (Logging, Automated Test Suite, Benchmarks, Report & Packaging)
 * Owner: Yug Malaviya (202401256)
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

static pthread_mutex_t g_logging_mutex = PTHREAD_MUTEX_INITIALIZER;
static LogLevel g_min_log_level = LOG_INFO;

static const char *level_to_string(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        default:        return "INFO ";
    }
}

void set_min_log_level(LogLevel min_level) {
    pthread_mutex_lock(&g_logging_mutex);
    g_min_log_level = min_level;
    pthread_mutex_unlock(&g_logging_mutex);
}

uint64_t get_monotonic_time_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0ULL;
    }
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

void log_event(LogLevel level, const char *module, const char *fmt, ...) {
    if (fmt == NULL) {
        return;
    }

    pthread_mutex_lock(&g_logging_mutex);
    if (level < g_min_log_level) {
        pthread_mutex_unlock(&g_logging_mutex);
        return;
    }

    struct timespec now_ts;
    struct tm tm_info;
    char time_buf[32];
    long msec = 0;

    if (clock_gettime(CLOCK_REALTIME, &now_ts) == 0) {
        localtime_r(&now_ts.tv_sec, &tm_info);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
        msec = now_ts.tv_nsec / 1000000L;
    } else {
        strncpy(time_buf, "0000-00-00 00:00:00", sizeof(time_buf) - 1);
        time_buf[sizeof(time_buf) - 1] = '\0';
    }

    const char *mod_tag = (module != NULL && module[0] != '\0') ? module : "CORE";

    fprintf(stderr, "%s.%03ld [%s] [%s] ",
            time_buf, msec, level_to_string(level), mod_tag);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    size_t fmt_len = strlen(fmt);
    if (fmt_len == 0 || fmt[fmt_len - 1] != '\n') {
        fputc('\n', stderr);
    }
    fflush(stderr);

    pthread_mutex_unlock(&g_logging_mutex);
}

int record_benchmark_metric(const BenchmarkRecord *record, const char *csv_path) {
    if (record == NULL || csv_path == NULL || csv_path[0] == '\0') {
        return -1;
    }

    pthread_mutex_lock(&g_logging_mutex);

    struct stat st;
    bool need_header = (stat(csv_path, &st) != 0 || st.st_size == 0);

    FILE *fp = fopen(csv_path, "a");
    if (fp == NULL) {
        pthread_mutex_unlock(&g_logging_mutex);
        return -1;
    }

    if (need_header) {
        fprintf(fp,
                "run_id,case_name,topic,num_clients,num_connections,failure_prob,"
                "useful_bytes,redundant_bytes,total_socket_bytes,elapsed_sec,"
                "throughput_mbps,overhead_ratio_percent,retry_count\n");
    }

    double elapsed = record->elapsed_sec;
    if (elapsed <= 0.0 && record->end_monotonic_ns > record->start_monotonic_ns) {
        elapsed = (double)(record->end_monotonic_ns - record->start_monotonic_ns) / 1e9;
    }

    double throughput = record->throughput_mbps;
    if (throughput <= 0.0 && elapsed > 0.0) {
        throughput = ((double)record->useful_bytes / (1024.0 * 1024.0)) / elapsed;
    }

    double overhead = record->overhead_ratio_percent;
    if (overhead <= 0.0 && record->useful_bytes > 0 &&
        record->total_socket_bytes >= record->useful_bytes) {
        overhead = ((double)(record->total_socket_bytes - record->useful_bytes) /
                    (double)record->useful_bytes) * 100.0;
    }

    fprintf(fp,
            "%u,%s,%s,%u,%u,%.4f,%lu,%lu,%lu,%.6f,%.4f,%.4f,%u\n",
            record->run_id,
            record->case_name[0] ? record->case_name : "unknown",
            record->topic[0] ? record->topic : "unknown",
            record->num_clients,
            record->num_connections,
            record->failure_prob,
            (unsigned long)record->useful_bytes,
            (unsigned long)record->redundant_bytes,
            (unsigned long)record->total_socket_bytes,
            elapsed,
            throughput,
            overhead,
            record->retry_count);

    fflush(fp);
    fclose(fp);

    pthread_mutex_unlock(&g_logging_mutex);
    return 0;
}
