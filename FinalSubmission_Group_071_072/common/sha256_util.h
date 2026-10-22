/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/sha256_util.h
 * Description: Application-Level SHA-256 File Integrity Verification Interface.
 * ============================================================================
 */

#ifndef SHA256_UTIL_H
#define SHA256_UTIL_H

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

#define SHA256_DIGEST_LENGTH 32
#define SHA256_HEX_STRING_LENGTH 65

/**
 * Computes the SHA-256 digest of a file at `filepath`.
 *
 * Reads file in bounded 64 KiB buffers using OpenSSL EVP so that files of any
 * size (including empty 0-byte files and large multi-gigabyte files) are
 * hashed with constant bounded memory overhead.
 *
 * Parameters:
 *   filepath : Canonical or relative path to the file on disk.
 *   digest   : Destination buffer receiving exactly 32 bytes of raw digest.
 *
 * Returns:
 *    0 on success.
 *   -1 if file cannot be opened, read fails, or OpenSSL EVP fails.
 */
int compute_file_sha256(const char *filepath, uint8_t digest[SHA256_DIGEST_LENGTH]);

/**
 * Formats a 32-byte binary SHA-256 digest into a 64-character lowercase hex
 * string followed by a null terminator.
 *
 * Parameters:
 *   digest  : 32-byte raw binary digest.
 *   hex_out : Destination buffer of at least SHA256_HEX_STRING_LENGTH (65) bytes.
 */
void sha256_to_hex(const uint8_t digest[SHA256_DIGEST_LENGTH], char hex_out[SHA256_HEX_STRING_LENGTH]);

/**
 * Parses a 64-character hexadecimal string into a 32-byte binary SHA-256 digest.
 *
 * Parameters:
 *   hex_in     : 64-character null-terminated hex string.
 *   digest_out : Destination buffer receiving 32 raw bytes.
 *
 * Returns:
 *    0 on success.
 *   -1 on malformed hex string (wrong length or non-hex characters).
 */
int sha256_from_hex(const char *hex_in, uint8_t digest_out[SHA256_DIGEST_LENGTH]);

/**
 * Compares two 32-byte binary digests for exact equality.
 *
 * Returns:
 *   1 if digests match exactly.
 *   0 if digests differ.
 */
int sha256_equal(const uint8_t digest_a[SHA256_DIGEST_LENGTH], const uint8_t digest_b[SHA256_DIGEST_LENGTH]);

#ifdef __cplusplus
}
#endif

#endif /* SHA256_UTIL_H */
