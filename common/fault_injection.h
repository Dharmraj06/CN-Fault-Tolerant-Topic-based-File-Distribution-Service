#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------------
 * Fault Injection Configuration & Control
 * ---------------------------------------------------------------------------- */

/**
 * init_fault_injection:
 * Initializes the global probabilistic fault injection engine.
 *
 * Validation rules:
 *   - failure_prob must strictly satisfy 0.0 <= failure_prob <= 1.0.
 *   - If seed is 0, the engine uses system time combined with process ID
 *     to produce non-deterministic random sequences.
 *
 * Parameters:
 *   failure_prob : Probability of injecting a disconnect before a data chunk (0.0 to 1.0).
 *   seed         : Base random seed for PRNG reproducibility (0 for automatic seeding).
 *
 * Return value:
 *    0 on success.
 *   -1 if failure_prob is out of range (< 0.0 or > 1.0).
 */
int init_fault_injection(double failure_prob, unsigned int seed);

/**
 * parse_failure_probability:
 * Validates and parses a command-line string representing failure probability.
 *
 * Validation rules:
 *   - Rejects NULL or empty strings.
 *   - Rejects strings with leading/trailing non-numeric characters (except valid sign/decimal).
 *   - Rejects values < 0.0 or > 1.0.
 *   - Rejects NaN and infinity.
 *
 * Parameters:
 *   str      : Input string (e.g., "0.05", "0.0", "1.0").
 *   out_prob : Pointer to store parsed double value.
 *
 * Return value:
 *    0 on valid float in [0.0, 1.0].
 *   -1 on syntax error or out-of-range value.
 */
int parse_failure_probability(const char *str, double *out_prob);

/**
 * should_inject_fault:
 * Thread-safe check to determine if the current chunk transmission should be
 * probabilistically terminated.
 *
 * Behavior:
 *   - If failure_prob <= 0.0: always returns false.
 *   - If failure_prob >= 1.0: always returns true.
 *   - Otherwise: generates a pseudo-random double in [0.0, 1.0) using thread-safe
 *     rand_r() with per-thread PRNG state and returns (r < failure_prob).
 *
 * Return value:
 *   true if a fault should be injected; false otherwise.
 */
bool should_inject_fault(void);

/**
 * execute_fault_disconnect:
 * Immediately closes the socket descriptor to simulate an abrupt network drop
 * or server crash, without transmitting any closing or protocol frames.
 *
 * Logs the termination event to stderr with a timestamp, socket descriptor,
 * and caller-provided context string (e.g., topic, file name, byte offset).
 *
 * Parameters:
 *   socket_fd   : Active socket descriptor to forcefully close.
 *   context_msg : Contextual description of the transfer state at disconnection.
 */
void execute_fault_disconnect(int socket_fd, const char *context_msg);

/**
 * get_failure_probability:
 * Retrieves the currently configured failure probability.
 *
 * Return value:
 *   Configured probability in [0.0, 1.0].
 */
double get_failure_probability(void);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_INJECTION_H */
