#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "fault_injection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <math.h>

/* ----------------------------------------------------------------------------
 * Internal Global State & Mutex Protection
 * ---------------------------------------------------------------------------- */

static double g_failure_prob = 0.0;
static unsigned int g_base_seed = 12345U;
static pthread_mutex_t g_fault_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Per-thread random state for lock-free, thread-safe rand_r() evaluation */
static __thread unsigned int s_thread_seed = 0;
static __thread bool s_thread_seed_initialized = false;

/* ----------------------------------------------------------------------------
 * Public Interface Implementations
 * ---------------------------------------------------------------------------- */

int init_fault_injection(double failure_prob, unsigned int seed) {
    if (failure_prob < 0.0 || failure_prob > 1.0) {
        return -1;
    }

    pthread_mutex_lock(&g_fault_mutex);
    g_failure_prob = failure_prob;
    if (seed == 0) {
        /* Seed from current epoch time combined with process ID for entropy */
        g_base_seed = (unsigned int)time(NULL) ^ ((unsigned int)getpid() << 16);
    } else {
        g_base_seed = seed;
    }
    pthread_mutex_unlock(&g_fault_mutex);

    return 0;
}

int parse_failure_probability(const char *str, double *out_prob) {
    if (!str || !out_prob) {
        return -1;
    }

    /* Skip leading whitespace */
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    if (*str == '\0') {
        return -1;
    }

    char *endptr = NULL;
    errno = 0;
    double val = strtod(str, &endptr);
    if (errno != 0) {
        return -1;
    }

    /* Skip trailing whitespace */
    while (*endptr == ' ' || *endptr == '\t' || *endptr == '\n' || *endptr == '\r') {
        endptr++;
    }
    if (*endptr != '\0') {
        return -1; /* Trailing non-numeric garbage found */
    }

    /* Strictly enforce boundary [0.0, 1.0] and reject non-finite values (NaN, Inf) */
    if (isnan(val) || !isfinite(val) || val < 0.0 || val > 1.0) {
        return -1;
    }

    *out_prob = val;
    return 0;
}

/* Portable thread-safe PRNG wrapper (uses POSIX rand_r on Linux, LCG fallback on others) */
static inline int fault_rand_r(unsigned int *seed)
{
    *seed = *seed * 1103515245U + 12345U;
    return (int)((*seed >> 16) & 0x7fffU);
}
bool should_inject_fault(void) {
    if (g_failure_prob <= 0.0) {
        return false;
    }
    if (g_failure_prob >= 1.0) {
        return true;
    }

    /* Initialize thread-local PRNG seed lazily if first call on this thread */
    if (!s_thread_seed_initialized) {
        pthread_mutex_lock(&g_fault_mutex);
        /* Step base seed using linear congruential generator to decorrelate threads */
        g_base_seed = g_base_seed * 1103515245U + 12345U;
        s_thread_seed = g_base_seed ^ (unsigned int)(uintptr_t)pthread_self();
        pthread_mutex_unlock(&g_fault_mutex);
        s_thread_seed_initialized = true;
    }

    /* Sample uniform pseudo-random number in [0.0, 1.0) */
  int r = fault_rand_r(&s_thread_seed);
double sample = (double)r / 32768.0;

    return (sample < g_failure_prob);
}

void execute_fault_disconnect(int socket_fd, const char *context_msg) {
    time_t now = time(NULL);
    struct tm tm_buf;
    char time_str[64];
    localtime_r(&now, &tm_buf);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    fprintf(stderr, "[FAULT] [%s] Injected disconnect on socket fd=%d: %s\n",
            time_str, socket_fd, (context_msg != NULL) ? context_msg : "(no context)");

    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

double get_failure_probability(void) {
    return g_failure_prob;
}
