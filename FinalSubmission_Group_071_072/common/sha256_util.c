/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/sha256_util.c
 * Description: Application-Level SHA-256 File Integrity Verification Implementation.
 * Uses OpenSSL EVP API for standard cryptographic file hashing.
 * ============================================================================
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "sha256_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <openssl/evp.h>

#define HASH_READ_BUFFER_SIZE (64 * 1024)

int compute_file_sha256(const char *filepath, uint8_t digest[SHA256_DIGEST_LENGTH])
{
    FILE *fp;
    EVP_MD_CTX *ctx;
    unsigned char *buffer;
    size_t bytes_read;
    unsigned int digest_len = 0;
    int success = 0;

    if (filepath == NULL || digest == NULL) {
        return -1;
    }

    fp = fopen(filepath, "rb");
    if (fp == NULL) {
        return -1;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        fclose(fp);
        return -1;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    buffer = malloc(HASH_READ_BUFFER_SIZE);
    if (buffer == NULL) {
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    while ((bytes_read = fread(buffer, 1, HASH_READ_BUFFER_SIZE, fp)) > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytes_read) != 1) {
            free(buffer);
            EVP_MD_CTX_free(ctx);
            fclose(fp);
            return -1;
        }
    }

    if (ferror(fp)) {
        free(buffer);
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    free(buffer);
    fclose(fp);

    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) == 1 && digest_len == SHA256_DIGEST_LENGTH) {
        success = 1;
    }

    EVP_MD_CTX_free(ctx);
    return success ? 0 : -1;
}

void sha256_to_hex(const uint8_t digest[SHA256_DIGEST_LENGTH], char hex_out[SHA256_HEX_STRING_LENGTH])
{
    static const char hex_chars[] = "0123456789abcdef";
    size_t i;

    if (digest == NULL || hex_out == NULL) {
        return;
    }

    for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hex_out[i * 2]     = hex_chars[(digest[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[digest[i] & 0x0F];
    }
    hex_out[SHA256_DIGEST_LENGTH * 2] = '\0';
}

static int hex_char_to_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int sha256_from_hex(const char *hex_in, uint8_t digest_out[SHA256_DIGEST_LENGTH])
{
    size_t i;

    if (hex_in == NULL || digest_out == NULL) {
        return -1;
    }

    if (strlen(hex_in) != SHA256_DIGEST_LENGTH * 2) {
        return -1;
    }

    for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        int hi = hex_char_to_val(hex_in[i * 2]);
        int lo = hex_char_to_val(hex_in[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        digest_out[i] = (uint8_t)((hi << 4) | lo);
    }

    return 0;
}

int sha256_equal(const uint8_t digest_a[SHA256_DIGEST_LENGTH], const uint8_t digest_b[SHA256_DIGEST_LENGTH])
{
    if (digest_a == NULL || digest_b == NULL) {
        return 0;
    }
    return memcmp(digest_a, digest_b, SHA256_DIGEST_LENGTH) == 0;
}
