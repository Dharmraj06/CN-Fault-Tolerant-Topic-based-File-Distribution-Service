/**
 * ============================================================================
 * IT305 Computer Networks Lab - Autumn 2026
 * Project: Fault-Tolerant Topic-Based File Distribution Service
 *
 * File: common/file_transfer.c
 * Description: Bounded Chunk File Streaming and Safe Disk I/O Implementation.
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

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "file_transfer.h"
#include "topic_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ----------------------------------------------------------------------------
 * Path Sanitization & Directory Management
 * ---------------------------------------------------------------------------- */

FileTransferError create_directory_recursive(const char *dir_path) {
    if (!dir_path || *dir_path == '\0') {
        return FT_ERR_INVALID_ARG;
    }

    size_t len = strlen(dir_path);
    if (len >= FT_MAX_PATH_LEN) {
        return FT_ERR_INVALID_ARG;
    }

    char tmp[FT_MAX_PATH_LEN];
    memcpy(tmp, dir_path, len + 1);

    /* Strip trailing slashes, preserving root '/' if present */
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        len--;
    }

    /* Iterate through path components */
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            struct stat st;
            if (stat(tmp, &st) != 0) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return FT_ERR_DIR_CREATE;
                }
            } else if (!S_ISDIR(st.st_mode)) {
                return FT_ERR_DIR_CREATE; /* Path component exists but is not a directory */
            }
            *p = '/';
        }
    }

    /* Create the final directory component */
    struct stat st;
    if (stat(tmp, &st) != 0) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return FT_ERR_DIR_CREATE;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        return FT_ERR_DIR_CREATE;
    }

    return FT_OK;
}

FileTransferError validate_filename(const char *filename) {
    if (!filename || *filename == '\0') {
        return FT_ERR_INVALID_ARG;
    }

    size_t len = strlen(filename);
    if (len > MAX_FILENAME_LEN) {
        return FT_ERR_PATH_ESCAPE;
    }

    /* Reject exact "." or ".." directory traversal entries */
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return FT_ERR_PATH_ESCAPE;
    }

    /* Reject path separators, control characters, and non-printable characters */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)filename[i];
        if (c == '/' || c == '\\') {
            return FT_ERR_PATH_ESCAPE;
        }
        if (c < 32 || c == 127) {
            return FT_ERR_PATH_ESCAPE;
        }
    }

    return FT_OK;
}

FileTransferError build_destination_path(const char *output_dir,
                                         const char *topic,
                                         const char *filename,
                                         char *out_path,
                                         size_t out_path_size) {
    if (!output_dir || !topic || !filename || !out_path || out_path_size == 0) {
        return FT_ERR_INVALID_ARG;
    }

    /* Validate filename */
    FileTransferError fterr = validate_filename(filename);
    if (fterr != FT_OK) {
        return fterr;
    }

    /* Validate topic name against path traversal */
    if (validate_topic_name(topic) != TOPIC_OK) {
        return FT_ERR_PATH_ESCAPE;
    }

    /* Reject traversal escapes in output_dir */
    if (strstr(output_dir, "..") != NULL) {
        return FT_ERR_PATH_ESCAPE;
    }

    /* Build formatted path: <output_dir>/<topic>/<filename> */
    int written = snprintf(out_path, out_path_size, "%s/%s/%s", output_dir, topic, filename);
    if (written < 0 || (size_t)written >= out_path_size) {
        return FT_ERR_WRITE;
    }

    return FT_OK;
}

/* ----------------------------------------------------------------------------
 * Safe File Descriptor Operations
 * ---------------------------------------------------------------------------- */

int open_file_for_reading(const char *path, uint64_t *out_file_size) {
    if (!path) {
        return FT_ERR_INVALID_ARG;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return FT_ERR_FILE_OPEN;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return FT_ERR_FILE_OPEN;
    }

    if (!S_ISREG(st.st_mode)) {
        /* Reject directories, devices, sockets, and FIFOs */
        close(fd);
        return FT_ERR_FILE_OPEN;
    }

    if (out_file_size) {
        *out_file_size = (uint64_t)st.st_size;
    }

    return fd;
}

int open_file_for_writing(const char *path, bool truncate) {
    if (!path) {
        return FT_ERR_INVALID_ARG;
    }

    int flags = O_WRONLY | O_CREAT;
    if (truncate) {
        flags |= O_TRUNC;
    }

    int fd = open(path, flags, 0644);
    if (fd < 0) {
        return FT_ERR_FILE_OPEN;
    }

    return fd;
}

FileTransferError close_file(int *fd) {
    if (!fd || *fd < 0) {
        return FT_ERR_INVALID_ARG;
    }

    int target = *fd;
    *fd = -1; /* Invalidate caller descriptor immediately to prevent double-close */

    while (1) {
        if (close(target) == 0) {
            return FT_OK;
        }
        if (errno == EINTR) {
            continue;
        }
        return FT_ERR_CLOSE;
    }
}

/* ----------------------------------------------------------------------------
 * Bounded Chunk Reading & Writing
 * ---------------------------------------------------------------------------- */

ssize_t read_chunk_from_file(int fd, void *buf, size_t max_bytes) {
    if (fd < 0 || !buf || max_bytes == 0) {
        return (ssize_t)FT_ERR_INVALID_ARG;
    }

    /* Enforce bounded chunk read limit */
    size_t bytes_to_read = max_bytes;
    if (bytes_to_read > FT_CHUNK_SIZE) {
        bytes_to_read = FT_CHUNK_SIZE;
    }

    while (1) {
        ssize_t n = read(fd, buf, bytes_to_read);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return (ssize_t)FT_ERR_READ;
        }
        return n; /* 0 indicates EOF, > 0 indicates bytes read */
    }
}

FileTransferError write_chunk_to_file(int fd, const void *buf, size_t bytes_to_write) {
    if (fd < 0 || (!buf && bytes_to_write > 0)) {
        return FT_ERR_INVALID_ARG;
    }

    if (bytes_to_write == 0) {
        return FT_OK;
    }

    const char *ptr = (const char *)buf;
    size_t total_written = 0;

    while (total_written < bytes_to_write) {
        ssize_t n = write(fd, ptr + total_written, bytes_to_write - total_written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return FT_ERR_WRITE;
        }
        if (n == 0) {
            return FT_ERR_WRITE;
        }
        total_written += (size_t)n;
    }

    return FT_OK;
}

FileTransferError sync_file_to_disk(int fd) {
    if (fd < 0) {
        return FT_ERR_INVALID_ARG;
    }

    while (1) {
        if (fsync(fd) == 0) {
            return FT_OK;
        }
        if (errno == EINTR) {
            continue;
        }
        return FT_ERR_SYNC;
    }
}

FileTransferError truncate_and_seek(int fd, uint64_t offset) {
    if (fd < 0) {
        return FT_ERR_INVALID_ARG;
    }

    if (ftruncate(fd, (off_t)offset) != 0) {
        return FT_ERR_TRUNCATE;
    }

    if (lseek(fd, (off_t)offset, SEEK_SET) == (off_t)-1) {
        return FT_ERR_SEEK;
    }

    return FT_OK;
}

/* ----------------------------------------------------------------------------
 * High-Level Protocol Streaming Helpers
 * ---------------------------------------------------------------------------- */

FileTransferError send_file_chunks(int sock_fd,
                                   int file_fd,
                                   uint64_t file_size,
                                   uint64_t start_offset) {
    if (sock_fd < 0 || file_fd < 0 || start_offset > file_size) {
        return FT_ERR_INVALID_ARG;
    }

    if (start_offset > 0) {
        if (lseek(file_fd, (off_t)start_offset, SEEK_SET) == (off_t)-1) {
            return FT_ERR_SEEK;
        }
    }

    /* 0-byte file: no chunks to transmit */
    if (file_size == 0 || start_offset == file_size) {
        return FT_OK;
    }

    char chunk_buf[FT_CHUNK_SIZE];
    uint64_t total_sent = start_offset;

    while (total_sent < file_size) {
        size_t bytes_to_read = FT_CHUNK_SIZE;
        uint64_t remaining = file_size - total_sent;
        if (remaining < (uint64_t)bytes_to_read) {
            bytes_to_read = (size_t)remaining;
        }

        ssize_t n_read = read_chunk_from_file(file_fd, chunk_buf, bytes_to_read);
        if (n_read < 0) {
            return FT_ERR_READ;
        }
        if (n_read == 0) {
            /* Unexpected premature EOF on local disk */
            return FT_ERR_READ;
        }

        /* Transmit chunk frame over socket using frozen protocol API */
        if (send_frame(sock_fd, MSG_FILE_DATA, FLAG_NONE, chunk_buf, (uint64_t)n_read) != 0) {
            return FT_ERR_NETWORK;
        }

        total_sent += (uint64_t)n_read;
    }

    return FT_OK;
}

FileTransferError receive_file_chunks(int sock_fd,
                                      int file_fd,
                                      uint64_t file_size) {
    if (sock_fd < 0 || file_fd < 0) {
        return FT_ERR_INVALID_ARG;
    }

    /* 0-byte file: no chunks to receive from socket */
    if (file_size == 0) {
        return FT_OK;
    }

    char chunk_buf[FT_CHUNK_SIZE];
    uint64_t total_received = 0;

    while (total_received < file_size) {
        FrameHeader hdr;
        int rc = recv_frame_header(sock_fd, &hdr);
        if (rc != 0) {
            return FT_ERR_NETWORK;
        }

        if (hdr.type != MSG_FILE_DATA) {
            return FT_ERR_NETWORK;
        }

        if (hdr.payload_len == 0 || hdr.payload_len > FT_CHUNK_SIZE) {
            return FT_ERR_NETWORK;
        }

        if (total_received + hdr.payload_len > file_size) {
            return FT_ERR_NETWORK;
        }

        rc = recv_frame_payload(sock_fd, &hdr, chunk_buf, sizeof(chunk_buf));
        if (rc != 0) {
            return FT_ERR_NETWORK;
        }

        FileTransferError werr = write_chunk_to_file(file_fd, chunk_buf, (size_t)hdr.payload_len);
        if (werr != FT_OK) {
            return werr;
        }

        total_received += hdr.payload_len;
    }

    return FT_OK;
}

const char* file_transfer_error_to_string(FileTransferError err) {
    switch (err) {
        case FT_OK:              return "Success";
        case FT_ERR_INVALID_ARG: return "Invalid argument or parameter";
        case FT_ERR_PATH_ESCAPE: return "Path traversal or escape attempt detected";
        case FT_ERR_DIR_CREATE:  return "Failed to create directory recursively";
        case FT_ERR_FILE_OPEN:   return "Failed to open file";
        case FT_ERR_READ:        return "Disk read error";
        case FT_ERR_WRITE:       return "Disk write error (or disk full)";
        case FT_ERR_SEEK:        return "File seek error";
        case FT_ERR_TRUNCATE:    return "File truncate error";
        case FT_ERR_SYNC:        return "File sync to disk error";
        case FT_ERR_CLOSE:       return "File close error";
        case FT_ERR_NETWORK:     return "Network protocol framing error";
        default:                 return "Unknown file transfer error";
    }
}
