/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/file_transfer.h
 * Description: Bounded Chunk File Streaming and Safe Disk I/O Interface.
 *
 * Phase: Phase 2 (Bounded File Streaming Engine & Part 1 Baseline Client)
 * Status: Implementation Complete
 *
 * Source of Truth Hierarchy:
 *   - Tier 1: IT305 Assignment Specification (§Part 1, §CLI)
 *   - Tier 2: Instructor Clarifications (§Page 3)
 *   - Tier 3: Approved Design Document Group 071_072 (§8.1, §8.2, §9.1, §9.2)
 *   - Tier 4: Master Execution Roadmap (PART2_TASKS_ROADMAP.md §4.32, §5 Task 2.1)
 * ============================================================================
 */

#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "protocol.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------------
 * File Transfer Constants & Limits
 * ---------------------------------------------------------------------------- */

/* Maximum path length buffer for constructing destination file paths */
#define FT_MAX_PATH_LEN 1024

/* Maximum chunk size for file transfers (matches frozen protocol DEFAULT_CHUNK_SIZE: 64 KiB) */
#define FT_CHUNK_SIZE DEFAULT_CHUNK_SIZE

/* ----------------------------------------------------------------------------
 * Error Codes
 * ---------------------------------------------------------------------------- */

typedef enum {
    FT_OK                 =  0, /* Operation completed successfully */
    FT_ERR_INVALID_ARG    = -1, /* NULL or invalid argument, or invalid buffer bounds */
    FT_ERR_PATH_ESCAPE    = -2, /* Destination path attempts directory traversal or escape */
    FT_ERR_DIR_CREATE     = -3, /* Failed to create directory recursively */
    FT_ERR_FILE_OPEN      = -4, /* Failed to open file (missing, permission denied, or invalid type) */
    FT_ERR_READ           = -5, /* Disk read error */
    FT_ERR_WRITE          = -6, /* Disk write error or disk full (ENOSPC) */
    FT_ERR_SEEK           = -7, /* lseek() failed */
    FT_ERR_TRUNCATE       = -8, /* ftruncate() failed */
    FT_ERR_SYNC           = -9, /* fsync() failed */
    FT_ERR_CLOSE          = -10,/* close() failed */
    FT_ERR_NETWORK        = -11 /* Socket framing or network I/O error */
} FileTransferError;

/* ----------------------------------------------------------------------------
 * Path Sanitization & Directory Management
 * ---------------------------------------------------------------------------- */

/**
 * create_directory_recursive:
 * Recursively creates all missing parent directories for the specified path,
 * using POSIX mkdir() with mode 0755.
 *
 * Parameters:
 *   dir_path : Destination directory path to create.
 *
 * Return value:
 *   FT_OK              : Directory exists or was successfully created.
 *   FT_ERR_INVALID_ARG : dir_path is NULL or empty.
 *   FT_ERR_DIR_CREATE  : System call failed (permission denied, read-only filesystem, etc.).
 */
FileTransferError create_directory_recursive(const char *dir_path);

/**
 * validate_filename:
 * Validates a filename to prevent directory traversal and injection.
 *
 * Security checks:
 *   - Rejects NULL or empty strings.
 *   - Rejects names exceeding MAX_FILENAME_LEN.
 *   - Rejects "." and ".." traversal tokens.
 *   - Rejects path separators ('/' and '\\').
 *   - Rejects non-printable ASCII or control characters.
 *
 * Parameters:
 *   filename : Candidate file name without path components.
 *
 * Return value:
 *   FT_OK              : Filename is safe.
 *   FT_ERR_PATH_ESCAPE : Filename contains forbidden characters or path traversal elements.
 */
FileTransferError validate_filename(const char *filename);

/**
 * build_destination_path:
 * Safely constructs `<output_dir>/<topic>/<filename>` in `out_path`.
 * Validates topic and filename against directory traversal before construction.
 *
 * Parameters:
 *   output_dir    : Client's target output directory.
 *   topic         : Topic name.
 *   filename      : Filename within topic.
 *   out_path      : Destination buffer.
 *   out_path_size : Capacity of destination buffer.
 *
 * Return value:
 *   FT_OK              : Destination path safely created.
 *   FT_ERR_INVALID_ARG : Invalid or NULL buffer pointers.
 *   FT_ERR_PATH_ESCAPE : Traversal detected in topic or filename.
 *   FT_ERR_WRITE       : Buffer size insufficient.
 */
FileTransferError build_destination_path(const char *output_dir,
                                         const char *topic,
                                         const char *filename,
                                         char *out_path,
                                         size_t out_path_size);

/* ----------------------------------------------------------------------------
 * Safe File Descriptor Operations
 * ---------------------------------------------------------------------------- */

/**
 * open_file_for_reading:
 * Safely opens a regular file for streaming with O_RDONLY.
 * Validates that the entry is a regular file via fstat() (rejecting directories,
 * FIFOs, and character/block devices) and retrieves the total file size.
 *
 * Parameters:
 *   path          : Path to regular file.
 *   out_file_size : Pointer to receive file size in bytes (may be NULL).
 *
 * Return value:
 *   File descriptor >= 0 on success; negative FileTransferError on failure.
 */
int open_file_for_reading(const char *path, uint64_t *out_file_size);

/**
 * open_file_for_writing:
 * Safely opens or creates a file for writing with mode 0644.
 *
 * Parameters:
 *   path     : Destination file path.
 *   truncate : If true, truncates existing file to 0 bytes (O_TRUNC).
 *
 * Return value:
 *   File descriptor >= 0 on success; negative FileTransferError on failure.
 */
int open_file_for_writing(const char *path, bool truncate);

/**
 * close_file:
 * Safely closes the open file descriptor, handling EINTR retries,
 * and resets *fd to -1 to prevent accidental double-close bugs.
 *
 * Parameters:
 *   fd : Pointer to open file descriptor variable.
 *
 * Return value:
 *   FT_OK              : File closed cleanly.
 *   FT_ERR_INVALID_ARG : fd is NULL or *fd < 0.
 *   FT_ERR_CLOSE       : close() failed.
 */
FileTransferError close_file(int *fd);

/* ----------------------------------------------------------------------------
 * Bounded Chunk Reading & Writing
 * ---------------------------------------------------------------------------- */

/**
 * read_chunk_from_file:
 * Reads up to `max_bytes` from file descriptor `fd` into `buf`.
 * The read size is capped at FT_CHUNK_SIZE (64 KiB) to enforce bounded memory usage.
 * Handles EINTR interrupts gracefully.
 *
 * Parameters:
 *   fd        : Open readable file descriptor.
 *   buf       : Destination buffer (must be at least max_bytes capacity).
 *   max_bytes : Maximum number of bytes to read (capped at FT_CHUNK_SIZE).
 *
 * Return value:
 *   Number of bytes read (> 0) on success.
 *   0 on clean EOF.
 *   negative FileTransferError on error.
 */
ssize_t read_chunk_from_file(int fd, void *buf, size_t max_bytes);

/**
 * write_chunk_to_file:
 * Writes exactly `bytes_to_write` from `buf` to file descriptor `fd`.
 * Loops on partial writes and retries on EINTR until all bytes are committed.
 *
 * Parameters:
 *   fd             : Open writable file descriptor.
 *   buf            : Source buffer.
 *   bytes_to_write : Exact number of bytes to write.
 *
 * Return value:
 *   FT_OK              : All bytes written successfully.
 *   FT_ERR_INVALID_ARG : Invalid arguments.
 *   FT_ERR_WRITE       : Write error (sets errno; captures ENOSPC).
 */
FileTransferError write_chunk_to_file(int fd, const void *buf, size_t bytes_to_write);

/**
 * sync_file_to_disk:
 * Flushes dirty file pages and metadata to durable storage using fsync().
 * Retries automatically on EINTR.
 *
 * Parameters:
 *   fd : Open writable file descriptor.
 *
 * Return value:
 *   FT_OK        : fsync() succeeded.
 *   FT_ERR_SYNC  : fsync() failed.
 */
FileTransferError sync_file_to_disk(int fd);

/**
 * truncate_and_seek:
 * Truncates file `fd` to `offset` bytes via ftruncate() and positions
 * the file pointer to `offset` via lseek(). Essential for Phase 2 Case 2
 * session rewind and conflict reconciliation.
 *
 * Parameters:
 *   fd     : Open writable file descriptor.
 *   offset : Byte offset to truncate to and seek.
 *
 * Return value:
 *   FT_OK            : Truncate and seek succeeded.
 *   FT_ERR_TRUNCATE  : ftruncate() failed.
 *   FT_ERR_SEEK      : lseek() failed.
 */
FileTransferError truncate_and_seek(int fd, uint64_t offset);

/* ----------------------------------------------------------------------------
 * High-Level Protocol Streaming Helpers
 * ---------------------------------------------------------------------------- */

/**
 * send_file_chunks:
 * Streams `file_size` bytes from `file_fd` (starting at `start_offset`)
 * over stream socket `sock_fd` as bounded MSG_FILE_DATA frames (up to 64 KiB each)
 * using the frozen send_frame() API.
 * Never loads the entire file into memory; uses a bounded 64 KiB buffer.
 *
 * Parameters:
 *   sock_fd      : Connected stream socket descriptor.
 *   file_fd      : Open readable file descriptor.
 *   file_size    : Total file size in bytes.
 *   start_offset : Starting byte offset (0 for baseline, >0 for resume).
 *
 * Return value:
 *   FT_OK           : Entire file stream transmitted.
 *   FT_ERR_SEEK     : lseek() to start_offset failed.
 *   FT_ERR_READ     : File read error.
 *   FT_ERR_NETWORK  : Socket transmission error.
 */
FileTransferError send_file_chunks(int sock_fd,
                                   int file_fd,
                                   uint64_t file_size,
                                   uint64_t start_offset);

/**
 * receive_file_chunks:
 * Receives incoming MSG_FILE_DATA frames from socket `sock_fd` and writes them
 * directly to `file_fd` via write_chunk_to_file() until exactly `file_size`
 * bytes have been received.
 *
 * Parameters:
 *   sock_fd   : Connected stream socket descriptor.
 *   file_fd   : Open writable file descriptor.
 *   file_size : Total expected file size in bytes.
 *
 * Return value:
 *   FT_OK           : Entire file data received and written.
 *   FT_ERR_WRITE    : Disk write error or ENOSPC.
 *   FT_ERR_NETWORK  : Socket error, premature EOF, or unexpected frame type.
 */
FileTransferError receive_file_chunks(int sock_fd,
                                      int file_fd,
                                      uint64_t file_size);

/**
 * file_transfer_error_to_string:
 * Translates a FileTransferError code into a human-readable diagnostic message.
 */
const char* file_transfer_error_to_string(FileTransferError err);

#ifdef __cplusplus
}
#endif

#endif /* FILE_TRANSFER_H */
