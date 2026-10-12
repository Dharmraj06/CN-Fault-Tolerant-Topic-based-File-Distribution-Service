# CODING STANDARDS & IMPLEMENTATION GUIDE
**Course**: IT305 Computer Networks Lab (Autumn 2026)  
**Academic Institution**: Dhirubhai Ambani Institute of Information and Communication Technology (DA-IICT)  
**Project**: Fault-Tolerant Topic-Based File Distribution Service  
**Target Audience**: Team Members (Jay Vaghela, Dharmraj Vaghela, Vivek Dodiya, Atharv Trivedi, Shlok Topaliya, Yug Malaviya)  
**Document Status**: Active Engineering Standard  

---

## 1. Introduction & Mentor Guidance

Welcome to the implementation phase of our Computer Networks laboratory project. This guide establishes the mandatory programming standards, engineering practices, and code quality expectations for all six members of our team.

As second- and third-year engineering students, our goal is to produce a **genuine, clean, reliable systems-programming project in POSIX C**. The code should reflect sound fundamentals: straightforward procedural control flow, robust socket handling, explicit resource management, and verifiable correctness.

### 1.1 The Golden Rules of This Project
1. **Simplicity over Cleverness**: A three-line `while` loop that any team member can immediately understand and explain to the lab instructor during the oral viva is vastly superior to a complex pointer macro or generic abstraction.
2. **Correctness is Non-Negotiable**: Simplicity does not mean cutting corners. Every return value from `socket()`, `recv()`, `open()`, and `malloc()` must be checked. Framing errors, partial transmissions, and connection drops must be handled with precision.
3. **No Unnecessary External Libraries**: The assignment requires pure standard C and POSIX APIs (`<sys/socket.h>`, `<unistd.h>`, `<pthread.h>`, `<dirent.h>`). Do not introduce third-party libraries, JSON parsers, or HTTP frameworks.
4. **Consistency Across All Four Directories**: Whether grading `Part1/`, `Part2_Case1/`, `Part2_Case2/`, or `Part2_Case2_Enhanced/`, the code formatting, naming conventions, error reporting, and framing logic must look like they were written by a unified engineering team.

---

## 2. Core Architectural & Coding Philosophy

Our codebase must adhere to the following principles throughout development:

### 2.1 Procedural C Design
- Write clean procedural C. Organize logic into well-defined functions with clear inputs and outputs.
- Avoid building complex "frameworks" or pseudo-object-oriented structures with function pointers unless specifically required for pluggable behaviors (such as the optional fault-injection hook).
- Do not create dozens of tiny 2-line helper functions that obscure program flow. If an operation happens in exactly one place and takes 5 lines of code, leave it inline with a brief explanatory comment.

### 2.2 Data Structures: Simple and Predictable
- Use standard `struct` definitions for grouping related attributes (e.g., `FrameHeader`, `FileEntry`, `TopicManifest`, `SessionEntry`).
- Prefer fixed-size arrays or simple dynamically allocated contiguous buffers.
- **Avoid linked lists** unless dynamic insertion or arbitrary deletion order provides a proven, significant benefit. For topic manifests and session tables, fixed-size arrays with explicit count fields (`MAX_FILES_PER_TOPIC = 1024`, `MAX_SESSIONS = 256`) are easier to debug, have zero pointer-chasing overhead, and eliminate memory fragmentation.

### 2.3 Transparent Control Flow
- Prefer familiar `for` and `while` loops, straightforward `if / else` conditionals, and predictable `switch` statements for protocol message dispatch.
- Avoid compressed one-liners, nested ternary operators (`? :`), and complex pointer arithmetic that obfuscate what the processor and network socket are actually doing.

---

## 3. Technical Specifications & Compiler Configuration

### 3.1 C Language Standard and Compiler Flags
All code across all four directories must compile cleanly using `gcc` under the **C11** (or POSIX-compatible C99) standard. Every `Makefile` must enforce strict warning detection:

```makefile
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -g
LDFLAGS = -pthread
```

#### Why these specific flags?
- `-std=c11`: Enforces modern standard C without compiler-specific non-standard dialects.
- `-Wall -Wextra`: Flags common bugs, uninitialized variables, unused function arguments, and signed/unsigned comparison mismatches.
- `-D_POSIX_C_SOURCE=200809L`: Exposes standard POSIX definitions (such as `pwrite()`, `sigaction`, `CLOCK_MONOTONIC`, and `fileno`) in C11 mode.
- `-pthread`: Configures POSIX threads correctly for both compiler preprocessor definitions and linker flags.
- `-g`: Generates debugging symbols necessary for debugging with `gdb` and profiling with `valgrind`.

### 3.2 Header File Discipline & Include Hierarchy
Every header file (`.h`) must contain standard include guards to prevent multiple inclusion. The include guard name must match the filename in uppercase:

```c
#ifndef PROTOCOL_H
#define PROTOCOL_H

/* Declarations */

#endif /* PROTOCOL_H */
```

In `.c` implementation files, order includes consistently from standard libraries down to local headers:

```c
/* 1. Standard C library headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

/* 2. POSIX and Linux system headers */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>

/* 3. Project-specific shared headers */
#include "protocol.h"
#include "topic_manager.h"
#include "logging.h"
```

### 3.3 Safe Handling of Strings and Buffers
Buffer overflows and missing null-terminators are the most frequent causes of crashes and grading deductions in student C projects. Follow these strict rules:

1. **Never use `strcpy()` or `strcat()`**: Use `strncpy()` or `snprintf()` with explicit buffer sizes.
2. **Guarantee Null-Termination**: When using `strncpy()`, always manually null-terminate the final byte:
   ```c
   strncpy(dest, src, sizeof(dest) - 1);
   dest[sizeof(dest) - 1] = '\0';
   ```
3. **Use `snprintf()` for String Formatting**:
   ```c
   char filepath[512];
   int n = snprintf(filepath, sizeof(filepath), "%s/%s", base_dir, filename);
   if (n < 0 || (size_t)n >= sizeof(filepath)) {
       // Path was truncated or formatting failed
       return -1;
   }
   ```
4. **Constant Pointer Parameters**: Whenever a function reads a string or buffer without modifying it, mark the pointer `const`:
   ```c
   int validate_topic_name(const char *topic);
   ssize_t send_all(int sockfd, const void *buf, size_t count);
   ```

### 3.4 Struct Packing for Wire Transmissions
Network protocols require that binary structures transmitted across the wire have an identical memory layout on every machine, regardless of compiler padding rules.

For wire structures, use `#pragma pack(push, 1)` and `#pragma pack(pop)`:

```c
#pragma pack(push, 1)
typedef struct {
    uint16_t magic;         /* 0x5446 ('TF') */
    uint8_t  version;       /* 1 */
    uint8_t  type;          /* FrameType enum */
    uint16_t flags;         /* Bitmask */
    uint16_t sequence_id;   /* Sequence / Transaction ID */
    uint64_t payload_len;   /* Exact byte count of payload */
} FrameHeader;
#pragma pack(pop)
```

> **Viva Note**: In your oral exam, if asked why `#pragma pack(1)` is necessary, explain: *"By default, C compilers insert padding bytes between struct members to align them to 4-byte or 8-byte CPU word boundaries. Packing forces the compiler to omit padding, ensuring that the struct is serialized as exactly 16 bytes over TCP across different compilers and architectures."*

### 3.5 Dynamic Memory Allocation Discipline
Memory leaks and dangling pointers lead to process crashes under repeated load. Adhere to these ownership principles:

1. **Clear Ownership**: Every piece of memory allocated with `malloc()` or `calloc()` must have an obvious owner and an explicit `free()` call.
2. **Worker Arguments**: In our thread-per-client model, the listener thread allocates worker arguments on the heap before launching the thread; the worker thread frees them immediately before exiting:
   ```c
   // In listener loop:
   WorkerArgs *args = malloc(sizeof(WorkerArgs));
   if (!args) { /* handle error */ }
   args->client_fd = client_fd;
   pthread_create(&thread_id, NULL, client_worker, args);

   // In client_worker function:
   void *client_worker(void *arg) {
       WorkerArgs *args = (WorkerArgs *)arg;
       int client_fd = args->client_fd;
       free(args); /* Freed immediately after copying needed fields */
       
       /* Proceed with client communication */
       return NULL;
   }
   ```
3. **Always Check `malloc` Returns**:
   ```c
   void *buf = malloc(chunk_size);
   if (!buf) {
       perror("malloc failed");
       return -1;
   }
   ```

---

## 4. Networking Standards: Sockets, Framing, & Wire Protocols

### 4.1 The Reality of TCP Stream Sockets (`SOCK_STREAM`)
Students often mistakenly treat TCP as a packet-based protocol where one `send()` corresponds to one `recv()`. **This is incorrect.** TCP is an unformatted continuous stream of bytes:
- A `send(sockfd, buf, 16384, 0)` may be split across multiple network packets and delivered to the receiver across several `recv()` calls (e.g., 4000 bytes, then 8000 bytes, then 4384 bytes).
- Two back-to-back `send()` calls of 8 bytes may be coalesced into a single 16-byte `recv()`.
- Binary files (JPEG, PNG) can contain `\n`, `\0`, or any arbitrary byte value, making text delimiters impossible.

Therefore, our system uses **fixed-length binary framing**.

### 4.2 Canonical Socket I/O Helpers: `send_all()` and `recv_all()`
All communication across all four project parts must pass through explicit `send_all()` and `recv_all()` loops. Never invoke bare `send()` or `recv()` directly in high-level business logic.

#### Implementation of `send_all()`
```c
// Send all requested bytes over the socket, looping until completed or error occurs.
ssize_t send_all(int sockfd, const void *buf, size_t count) {
    size_t total_sent = 0;
    const char *ptr = (const char *)buf;

    while (total_sent < count) {
        // MSG_NOSIGNAL prevents raising SIGPIPE if the remote end disconnected.
        ssize_t n = send(sockfd, ptr + total_sent, count - total_sent, MSG_NOSIGNAL);
        
        if (n < 0) {
            if (errno == EINTR) {
                // System call was interrupted by a signal; retry immediately.
                continue;
            }
            return -1; // Socket error or connection dropped
        }
        
        if (n == 0) {
            // Connection closed by peer
            return total_sent;
        }

        total_sent += n;
    }

    return (ssize_t)total_sent;
}
```

#### Implementation of `recv_all()`
```c
// Receive exactly count bytes into buf, looping until complete or EOF is encountered.
ssize_t recv_all(int sockfd, void *buf, size_t count) {
    size_t total_received = 0;
    char *ptr = (char *)buf;

    while (total_received < count) {
        ssize_t n = recv(sockfd, ptr + total_received, count - total_received, 0);
        
        if (n < 0) {
            if (errno == EINTR) {
                // Signal interrupted system call; retry.
                continue;
            }
            return -1; // Socket error
        }
        
        if (n == 0) {
            // Peer cleanly closed connection (EOF).
            return (ssize_t)total_received;
        }

        total_received += n;
    }

    return (ssize_t)total_received;
}
```

### 4.3 Endianness & Network Byte Order
Network byte order is **Big-Endian**. Modern x86_64 consumer machines are **Little-Endian**.
- For 16-bit integers: use `htons()` and `ntohs()`.
- For 32-bit integers: use `htonl()` and `ntohl()`.
- For 64-bit integers: standard C does not have `htonll` on all POSIX platforms. Use this standard, portable converter:

```c
// Convert a 64-bit integer between host and network byte order.
static inline uint64_t hton64(uint64_t val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)htonl((uint32_t)(val & 0xFFFFFFFF))) << 32) |
           (uint64_t)htonl((uint32_t)(val >> 32));
#else
    return val;
#endif
}

#define ntoh64(val) hton64(val)
```

### 4.4 The Two-Step Frame Reception Pattern
When receiving a frame, the receiver must always execute two explicit steps:
1. **Read Header**: Read exactly `sizeof(FrameHeader)` (16 bytes) using `recv_all()`.
2. **Validate & Read Payload**:
   - Verify `hdr.magic == ntohs(PROTOCOL_MAGIC)`. If incorrect, reject immediately.
   - Decode `payload_len = ntoh64(hdr.payload_len)`.
   - Validate `payload_len <= MAX_FRAME_PAYLOAD` (sanity guard against malicious/corrupt frames).
   - If `payload_len > 0`, call `recv_all(sockfd, payload_buf, payload_len)`.

```c
// Read a complete frame: header first, then payload.
int receive_frame(int sockfd, FrameHeader *out_hdr, void *payload_buf, size_t max_payload_len) {
    // Step 1: Read the 16-byte fixed header
    ssize_t res = recv_all(sockfd, out_hdr, sizeof(FrameHeader));
    if (res <= 0) {
        return (res == 0) ? 0 : -1; // 0 = EOF, -1 = Error
    }

    // Step 2: Validate magic number
    if (ntohs(out_hdr->magic) != PROTOCOL_MAGIC) {
        fprintf(stderr, "[ERROR] Invalid protocol magic: 0x%04X\n", ntohs(out_hdr->magic));
        return -2; // Protocol violation
    }

    uint64_t payload_len = ntoh64(out_hdr->payload_len);
    if (payload_len > max_payload_len) {
        fprintf(stderr, "[ERROR] Payload length %lu exceeds maximum buffer %zu\n", 
                payload_len, max_payload_len);
        return -3;
    }

    // Step 3: Read exact payload bytes
    if (payload_len > 0) {
        res = recv_all(sockfd, payload_buf, payload_len);
        if (res != (ssize_t)payload_len) {
            return -1; // Abrupt drop during payload
        }
    }

    return 1; // Success
}
```

### 4.5 Mandatory Signal Safety: Ignoring `SIGPIPE`
When writing to a TCP socket that has been closed by the peer, the OS delivers a `SIGPIPE` signal to the process. By default in Linux, `SIGPIPE` **terminates the entire application immediately**.

Every server and client executable in this project must disable default `SIGPIPE` behavior during initialization:

```c
#include <signal.h>

int main(int argc, char *argv[]) {
    // Ignore SIGPIPE so that writing to a closed socket returns -1 with errno=EPIPE
    // instead of abruptly killing the process.
    signal(SIGPIPE, SIG_IGN);
    
    /* ... rest of initialization ... */
}
```

---

## 5. File System & Disk I/O Standards

### 5.1 Directory Scanning & Manifest Generation
The server must inspect `<topics_root_dir>/<topic>` and assemble a sorted manifest of regular files.
- Use POSIX `opendir()`, `readdir()`, and `closedir()`.
- Use `stat()` to verify that each item is a regular file (`S_ISREG(st.st_mode)`).
- Explicitly ignore `.` (current directory), `..` (parent directory), subdirectories, and hidden files.
- Sort the resulting list of filenames lexicographically using `qsort()` and `strcmp()`.

```c
// Lexicographical comparison function for qsort()
static int compare_file_entries(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    return strcmp(fa->filename, fb->filename);
}
```

### 5.2 Path Traversal Security
A network service must never trust client-supplied path strings. If a client sends `GET ../../etc/shadow`, the server must reject it. Similarly, if a server sends a malicious filename `../../evil.sh`, the client must reject it.

Implement and use strict validation:

```c
// Validates that a topic name contains only safe alphanumeric characters, underscores, and dashes.
int validate_topic_name(const char *topic) {
    if (!topic || strlen(topic) == 0 || strlen(topic) >= MAX_TOPIC_NAME) {
        return 0; // Invalid
    }

    // Reject any attempt at directory traversal
    if (strstr(topic, "..") || strchr(topic, '/') || strchr(topic, '\\')) {
        return 0; // Malicious path detected
    }

    // Allow only alphanumeric characters, dashes, and underscores
    for (size_t i = 0; i < strlen(topic); i++) {
        char c = topic[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return 0;
        }
    }

    return 1; // Valid
}
```

### 5.3 File Streaming and Seeking: Why `pwrite()` is Preferred
When receiving file chunks across multiple threads (especially in `Part2_Case2_Enhanced`), standard `lseek()` followed by `write()` is **not thread-safe** if multiple threads share a file descriptor, because seeking changes the shared file offset.

Use `pwrite()` for writing received chunks:
```c
// Write chunk at an explicit file offset without altering file offset pointer
ssize_t n = pwrite(fd, data_buffer, chunk_len, file_offset);
```
`pwrite()` writes data at the specified offset atomically in a single system call, making it safe and robust.

### 5.4 Verifying File Integrity & Completeness
Upon receiving all chunks for a file, the client must verify that the file on disk matches the expected size:

```c
// Verify that the downloaded file on disk matches the manifest size.
int verify_file_size(const char *filepath, uint64_t expected_size) {
    struct stat st;
    if (stat(filepath, &st) < 0) {
        perror("stat failed");
        return -1;
    }

    if ((uint64_t)st.st_size != expected_size) {
        fprintf(stderr, "[ERROR] Size mismatch for %s: expected %lu bytes, found %ld bytes\n",
                filepath, expected_size, (long)st.st_size);
        return 0; // Corrupt / incomplete
    }

    return 1; // Verified match
}
```

---

## 6. Multithreading & Synchronization Standards

### 6.1 The Thread-Per-Client Model
For `Part1` (multi-threaded mode), `Part2_Case1`, `Part2_Case2`, and `Part2_Case2_Enhanced`, the server must handle concurrent clients using a **thread-per-client** architecture.

- The server listener thread executes `accept()` in a continuous loop.
- When a client connects, the listener allocates a worker argument struct on the heap and creates a thread with `pthread_create()`.
- Set the thread to detached mode using `pthread_detach(pthread_self())` inside the worker, or call `pthread_detach(thread_id)` in the listener, so the operating system automatically reclaims thread resources upon completion without requiring `pthread_join()`.

```c
// Standard Thread-per-Client Listener Loop
while (keep_running) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (errno == EINTR) continue;
        perror("accept failed");
        break;
    }

    WorkerArgs *args = malloc(sizeof(WorkerArgs));
    if (!args) {
        perror("malloc worker args failed");
        close(client_fd);
        continue;
    }
    
    args->client_fd = client_fd;
    args->client_addr = client_addr;

    pthread_t tid;
    if (pthread_create(&tid, NULL, client_worker, args) != 0) {
        perror("pthread_create failed");
        close(client_fd);
        free(args);
        continue;
    }

    pthread_detach(tid); // Ensure resources are freed when thread terminates
}
```

### 6.2 Mutex Discipline: The Cardinal Rule of Thread Synchronization
In multithreaded networking, shared state (such as the in-memory session table) must be protected by a `pthread_mutex_t`.

> [!CAUTION]
> **THE CARDINAL RULE OF MUTEX SYNCHRONIZATION**:  
> **NEVER perform network I/O (`send`, `recv`) or blocking disk I/O while holding a mutex lock!**

Holding a mutex while waiting for a network socket will freeze all other worker threads in the server. If a client stalls or network latency spikes, the entire server comes to an immediate halt.

#### Correct Pattern (Fine-Grained Locking)
```c
// 1. Acquire mutex to safely inspect or update shared memory
pthread_mutex_lock(&table->lock);

// 2. Read or update memory rapidly
SessionEntry *session = find_session(table, session_id);
uint64_t resume_offset = session->committed_file_offset;
uint32_t file_index = session->current_file_index;

// 3. Immediately release mutex!
pthread_mutex_unlock(&table->lock);

// 4. Perform slow disk read and network transmission without holding the mutex
stream_chunk_over_network(client_fd, file_index, resume_offset);
```

---

## 7. Session Management & Checkpoint Resumption (Part 2 Case 2)

### 7.1 Session State Machine
In `Part2_Case2`, the server maintains an in-memory session table to track client progress. A session advances through the following clear lifecycle states:

```
[ UNUSED ] ──(MSG_GET initial)──► [ ACTIVE ] ──(Injected Fault / Drop)──► [ INTERRUPTED ]
                                      │                                           │
                                      │                                (MSG_GET resume)
                                      │                                           │
                                      ▼                                           ▼
                              [ COMPLETED ] ◄────────────────────────────── [ ACTIVE ]
```

### 7.2 Session Table Structure
The session table is a fixed array of `SessionEntry` structures protected by a single mutex:

```c
typedef enum {
    SESSION_STATE_UNUSED      = 0,
    SESSION_STATE_ACTIVE      = 1,
    SESSION_STATE_INTERRUPTED = 2,
    SESSION_STATE_COMPLETED   = 3,
    SESSION_STATE_EXPIRED     = 4
} SessionStatus;

typedef struct {
    uint64_t      session_id;                  /* 64-bit unique identifier */
    SessionStatus status;                      /* Current session state */
    char          topic[MAX_TOPIC_NAME];       /* Topic being transferred */
    uint32_t      manifest_fingerprint;        /* CRC32 hash of topic files */
    uint32_t      current_file_index;          /* Index of file in progress */
    uint64_t      committed_file_offset;       /* Last acknowledged byte offset */
    uint32_t      completed_files_count;       /* Number of fully sent files */
    time_t        last_activity;               /* Timestamp for TTL expiration */
    uint64_t      useful_bytes_transferred;    /* Metrics tracking */
} SessionEntry;

typedef struct {
    SessionEntry    entries[MAX_SESSIONS];
    pthread_mutex_t lock;
} SessionTable;
```

### 7.3 Checkpoint Commit Semantics
1. **Server sends chunk**: Server reads 16 KB chunk from disk and sends `MSG_FILE_DATA(offset, len)`.
2. **Client writes chunk**: Client uses `pwrite()` to commit chunk to disk.
3. **Client acknowledges chunk**: Client sends `MSG_CHUNK_ACK(session_id, file_index, committed_offset)`.
4. **Server commits checkpoint**: Upon receiving `MSG_CHUNK_ACK`, the server locks the session mutex and updates `committed_file_offset`.
5. **If fault occurs**: If the connection drops before `MSG_CHUNK_ACK` is received by the server, the server does not commit the offset. Upon reconnection, the client resumes from the last server-committed offset. This guarantees zero file corruption and bounded retransmission.

---

## 8. Parallel TCP Transfer Standards (Part 2 Case 2 Enhanced)

In `Part2_Case2_Enhanced`, the client opens $N$ parallel TCP connections to the server to accelerate topic transfer.

### 8.1 Coordinator and Worker Architecture
Keep the design clean and procedural:
- **Master Connection (Coordinator)**: The client connects on a master socket, sends `MSG_GET`, and requests $N$ connections.
- **Work Partitioning**: The server partitions the topic's files and chunk ranges into discrete `WorkUnit` items (e.g., Unit 0 = File 0, Unit 1 = File 1, or ranges of large files).
- **Worker Threads**: The client launches $N$ worker threads. Each thread opens its own TCP connection to the server, issues `MSG_WORK_ASSIGN` to claim a work unit, streams data, writes to disk using `pwrite()`, and acknowledges progress.
- **Barrier Synchronization**: The client coordinator waits for all worker threads to complete using `pthread_join()`.
- **Fault Recovery in Parallel**: If worker connection 2 drops due to fault injection, only worker 2 reconnects and resumes its assigned work unit. Connections 1 and 3 continue transferring their units completely uninterrupted.

---

## 9. Commenting & Documentation Standards

Comments should be brief, useful, and understandable to a peer studying computer networks.

### 9.1 The "Why, Not What" Rule
Do not write comments that merely re-state the C syntax. Write comments that explain **networking intent, invariants, or why a decision was made**.

#### Poor Comments (Do Not Write These)
```c
// Increment i by 1
i++;

// Call socket function with AF_INET, SOCK_STREAM, 0
int s = socket(AF_INET, SOCK_STREAM, 0);

// If res is less than 0
if (res < 0) {
    // Print error
    printf("error\n");
}
```

#### Good Comments (Preferred Style)
```c
// Send the complete header, even if send() writes only part of it.
if (send_all(sockfd, &hdr, sizeof(hdr)) < 0) {
    // Remote peer dropped the connection during header transmission.
    return -1;
}

// Ignore SIGPIPE so broken socket writes return EPIPE instead of killing the process.
signal(SIGPIPE, SIG_IGN);

// Acquire mutex only to update checkpoint state; release immediately before socket I/O.
pthread_mutex_lock(&table->lock);
session->committed_file_offset += chunk_len;
pthread_mutex_unlock(&table->lock);
```

### 9.2 File Header Template
Keep file headers concise and informative:

```c
/**
 * topic_manager.c - Topic directory scanning, validation, and manifest generation.
 * Course: IT305 Computer Networks Lab (DA-IICT)
 * Authors: Group 071 + 072
 */
```

---

## 10. Naming, Formatting & Code Style Conventions

To ensure consistent style across all team members, follow these concrete rules:

### 10.1 Identifier Conventions
| Item | Style | Example |
| :--- | :--- | :--- |
| **Variables** | `snake_case` | `client_fd`, `bytes_sent`, `total_files` |
| **Functions** | `snake_case` | `send_all()`, `build_manifest()`, `session_create()` |
| **Struct Types** | `PascalCase` | `FrameHeader`, `TopicManifest`, `SessionEntry` |
| **Enums & Types** | `PascalCase` | `FrameType`, `SessionStatus`, `LogLevel` |
| **Constants & Macros**| `UPPER_SNAKE_CASE` | `DEFAULT_CHUNK_SIZE`, `MAX_TOPIC_NAME`, `PROTOCOL_MAGIC` |
| **Enum Values** | `UPPER_SNAKE_CASE` | `MSG_GET`, `MSG_FILE_DATA`, `SESSION_STATE_ACTIVE` |

### 10.2 Formatting and Layout
- **Indentation**: 4 spaces per indentation level. **Never use hard tabs**.
- **Brace Placement**: Use standard K&R / 1TBS style (opening brace on the same line as statements, closing brace on its own line):
  ```c
  if (bytes_read < 0) {
      perror("read error");
      return -1;
  } else {
      total_bytes += bytes_read;
  }
  ```
  For function definitions, place the opening brace on the same line or next line consistently:
  ```c
  int sanitize_filename(const char *input, char *output, size_t max_len) {
      /* body */
  }
  ```
- **Line Length**: Keep lines within 80 to 100 characters for readability.
- **Function Length**: Functions should generally not exceed 60–80 lines. If a function grows beyond 80 lines, separate logical steps into focused helper functions.

---

## 11. Error Handling Architecture & Viva-Friendly Cleanup

### 11.1 Checked System Calls Matrix
Every student must check return values for all POSIX and C library calls:

| System Call | Failure Condition | Action Required |
| :--- | :--- | :--- |
| `socket()` | `< 0` | Log error with `perror()`, return error code or exit. |
| `bind()` | `< 0` | Fatal error on server startup: `perror()`, exit with code 1. |
| `listen()` | `< 0` | Fatal error on server startup: `perror()`, exit with code 1. |
| `accept()` | `< 0` | If `errno == EINTR`, retry. Otherwise log error and continue listener loop. |
| `connect()` | `< 0` | Client network error: log error, trigger backoff retry loop. |
| `send()` | `< 0` | If `errno == EINTR`, retry. Otherwise return -1 (peer closed / broken pipe). |
| `recv()` | `== 0` | Orderly EOF: peer closed connection. Terminate transfer cleanly. |
| `recv()` | `< 0` | If `errno == EINTR`, retry. Otherwise return -1 (transmission error). |
| `malloc()` | `NULL` | Memory exhausted: log error, release resources, return error. |
| `open()` | `< 0` | File I/O error: log error, send `MSG_ERROR` to peer, clean up. |
| `read() / write()` | `< 0` | If `errno == EINTR`, retry. Otherwise handle disk I/O failure. |
| `pthread_create()` | `!= 0` | Thread creation failed: close client socket, free worker args. |

### 11.2 Error Classification & Recovery Table
When an error occurs, follow this decision matrix:

| Error Category | Example Scenario | Immediate Action |
| :--- | :--- | :--- |
| **Fatal Startup** | Port already in use (`bind` failed), root dir missing | Print clear error to `stderr`, terminate process with exit code 1. |
| **Client Disconnect** | Peer closed socket abruptly during transfer | Close socket descriptor, free client memory, terminate worker thread. **Do not terminate server!** |
| **Protocol Violation** | Header magic mismatch, oversized `payload_len` | Send `MSG_ERROR` if socket open, close socket, terminate client worker. |
| **Path Traversal Attack**| Client requested `../../etc/passwd` | Log security warning, reply with `ERR_INVALID_TOPIC_NAME`, close socket. |
| **Injected Fault** | Probabilistic disconnection in `Part2` | Server closes socket immediately. Client catches EOF/error and initiates reconnect. |

### 11.3 Viva-Friendly Resource Cleanup Pattern
Avoid deeply nested 5-level `if` statements for cleanup. In C, the standard, clean way to handle error cleanup without memory leaks is the **linear cleanup block** using `goto cleanup` or structured single-exit helpers:

```c
// Example of clean, viva-defensible resource cleanup
int transfer_single_file(int sockfd, const char *filepath) {
    int fd = -1;
    void *buffer = NULL;
    int status = -1;

    fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        goto cleanup;
    }

    buffer = malloc(DEFAULT_CHUNK_SIZE);
    if (!buffer) {
        perror("malloc failed");
        goto cleanup;
    }

    // Stream chunks...
    status = 0; // Success

cleanup:
    // Resources are freed in reverse order of acquisition
    if (buffer) {
        free(buffer);
    }
    if (fd >= 0) {
        close(fd);
    }
    return status;
}
```

---

## 12. File & Module Organization Across the Four Parts

To satisfy the automated grading script while sharing common code during development, our repository is organized as follows:

```
CN-Fault-Tolerant-Topic-based-File-Distribution-Service/
├── common/                  <-- Development source of truth
│   ├── protocol.h / .c      <-- Framing, serialization, send_all/recv_all
│   ├── topic_manager.h / .c <-- Topic scanning, manifests, path security
│   ├── file_transfer.h / .c <-- Chunked file I/O, pwrite, integrity
│   ├── fault_injection.h/.c <-- Probabilistic connection interruption
│   ├── session.h / .c       <-- In-memory session table & checkpointing
│   ├── logging.h / .c       <-- Thread-safe logging & CSV metrics
│   └── work_plan.h / .c     <-- Parallel work unit partitioning
│
├── Part1/                   <-- Deliverable 1 (Basic Sequential & MT)
│   ├── server.c
│   ├── client.c
│   └── Makefile             <-- Builds ./server and ./client independently
│
├── Part2_Case1/             <-- Deliverable 2 (Faults + Full Topic Restart)
│   ├── server.c
│   ├── client.c
│   └── Makefile             <-- Builds ./server and ./client independently
│
├── Part2_Case2/             <-- Deliverable 3 (Session Table + Checkpoint Resume)
│   ├── server.c
│   ├── client.c
│   └── Makefile             <-- Builds ./server and ./client independently
│
├── Part2_Case2_Enhanced/    <-- Deliverable 4 (Parallel TCP Connections)
│   ├── server.c
│   ├── client.c
│   └── Makefile             <-- Builds ./server and ./client independently
│
└── Results/                 <-- Deliverable 5 (CSV logs and plotted graphs)
```

### 12.1 Makefiles and Standalone Deliverables
Each directory (`Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`) must have a self-contained `Makefile` that:
1. Compiles the executable targets `./server` and `./client` directly by running `make`.
2. Includes a `clean` target that removes all compiled `.o` files and binaries.
3. Produces zero warnings under `gcc -Wall -Wextra`.

---

## 13. Practical Reference Implementations (Code Blueprints)

Below are three essential code blueprints illustrating exactly how the team's code should look.

### 13.1 Blueprint 1: Client Reconnect & Resumption Loop (Part 2 Case 2)

```c
// Client reconnect loop for Part 2 Case 2: Resume from last checkpoint
int client_download_topic_with_resume(const char *ip, int port, const char *topic, const char *out_dir) {
    uint64_t session_id = 0; // Initial transfer starts with session_id 0
    int retries = 0;
    const int max_retries = 10;
    bool completed = false;

    while (!completed && retries < max_retries) {
        // Step 1: Establish TCP connection
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket creation failed");
            return -1;
        }

        struct sockaddr_in srv_addr;
        memset(&srv_addr, 0, sizeof(srv_addr));
        srv_addr.sin_family = AF_INET;
        srv_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &srv_addr.sin_addr);

        if (connect(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
            perror("connect failed, waiting before retry...");
            close(sockfd);
            retries++;
            usleep(200000); // 200ms exponential/fixed backoff
            continue;
        }

        // Step 2: Send MSG_GET frame (passes session_id: 0 if first try, >0 if resuming)
        PayloadGet get_payload;
        memset(&get_payload, 0, sizeof(get_payload));
        get_payload.session_id = hton64(session_id);
        get_payload.topic_len = htons(strlen(topic));
        strncpy(get_payload.topic, topic, sizeof(get_payload.topic) - 1);

        if (send_frame(sockfd, MSG_GET, 0, 1, &get_payload, sizeof(get_payload)) < 0) {
            close(sockfd);
            retries++;
            continue;
        }

        // Step 3: Receive MSG_OK containing session_id and manifest
        FrameHeader hdr;
        if (recv_frame_header(sockfd, &hdr) <= 0 || hdr.type != MSG_OK) {
            close(sockfd);
            retries++;
            continue;
        }

        // Read manifest and update our session_id
        PayloadOkHeader ok_hdr;
        recv_all(sockfd, &ok_hdr, sizeof(ok_hdr));
        session_id = ntoh64(ok_hdr.session_id);

        // Step 4: Stream file chunks with checkpoint acknowledgments
        int res = client_receive_file_stream(sockfd, out_dir, session_id);
        if (res == 0) {
            // Transfer completed successfully!
            completed = true;
        } else {
            // Connection dropped or fault injected: reconnect and resume!
            printf("[CLIENT] Connection interrupted. Reconnecting with session ID %lu...\n", session_id);
            retries++;
            usleep(100000); // 100ms backoff
        }

        close(sockfd);
    }

    return completed ? 0 : -1;
}
```

### 13.2 Blueprint 2: Chunked File Streaming with Fault Injection Hook

```c
// Server chunked file streaming function
int stream_file_chunks(int sockfd, const char *filepath, uint32_t file_index,
                       uint64_t start_offset, uint64_t total_size,
                       FaultInjector *fi) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        return -1;
    }

    // Seek to resume offset if resuming a transfer
    if (start_offset > 0) {
        if (lseek(fd, (off_t)start_offset, SEEK_SET) < 0) {
            perror("lseek failed");
            close(fd);
            return -1;
        }
    }

    char buffer[DEFAULT_CHUNK_SIZE];
    uint64_t current_offset = start_offset;

    while (current_offset < total_size) {
        // Optional fault injection check before sending each chunk
        if (fi && should_inject_fault(fi)) {
            printf("[FAULT INJECTION] Simulating abrupt connection failure at offset %lu!\n", current_offset);
            close(fd);
            // Abrupt close causes RST or EOF on client side
            return -2;
        }

        size_t to_read = DEFAULT_CHUNK_SIZE;
        if (current_offset + to_read > total_size) {
            to_read = total_size - current_offset;
        }

        ssize_t bytes_read = read(fd, buffer, to_read);
        if (bytes_read <= 0) {
            break;
        }

        // Send chunk frame
        if (send_chunk_frame(sockfd, file_index, current_offset, buffer, (size_t)bytes_read) < 0) {
            close(fd);
            return -1; // Socket error
        }

        current_offset += bytes_read;
    }

    close(fd);
    return 0; // Successfully streamed all bytes
}
```

---

## 14. Viva Defense Guide: How to Defend Your Code in the Oral Exam

During the laboratory oral evaluation ("viva"), professors and TAs will ask probing questions to verify that every team member understands what the C code is doing at the operating system and networking layers.

Here are the top questions and the exact systems answers you should know:

### Q1: Why did you use `send_all()` and `recv_all()` instead of calling `send()` and `recv()` directly?
**Answer**: *"TCP is a stream-oriented protocol (`SOCK_STREAM`), not a message-oriented protocol. A single call to `recv()` might return fewer bytes than the server sent due to TCP packet fragmentation or receiver window sizing. Similarly, `send()` can perform partial writes if the socket send buffer is full. `send_all()` and `recv_all()` loop explicitly until all requested bytes are transferred, preventing message boundary corruption."*

### Q2: Why did you ignore `SIGPIPE`?
**Answer**: *"If a client drops its connection and the server attempts to call `send()`, the operating system's default behavior is to deliver a `SIGPIPE` signal to the server, which immediately terminates the process. By setting `signal(SIGPIPE, SIG_IGN)` or using the `MSG_NOSIGNAL` flag, `send()` simply returns `-1` with `errno = EPIPE`. This allows the server worker thread to cleanly close the connection and log the error without crashing the server."*

### Q3: Why is `#pragma pack(push, 1)` used for your network frame header?
**Answer**: *"Compilers automatically align struct members to 4- or 8-byte CPU memory boundaries by inserting hidden padding bytes. Across different architectures or compiler optimization levels, padding may differ. Packing ensures that the header structure has zero padding and matches the exact 16-byte binary wire specification on both sender and receiver."*

### Q4: Why must the session table mutex NOT be held during `send()` or `recv()`?
**Answer**: *"Holding a mutex during network I/O is a severe concurrency anti-pattern. If a worker thread holds the mutex while sending a file chunk over a slow network or waiting on a slow client, every other worker thread trying to update or read sessions will block on that mutex. Mutex locks must only be held for microsecond-level in-memory state updates, then released immediately before any socket or disk I/O."*

### Q5: Why is `pwrite()` safer than `lseek()` + `write()` in multi-threaded file transfers?
**Answer**: *"In multi-threaded programs where multiple threads might write to different parts of the same file (such as in Part 2 Case 2 Enhanced), `lseek()` modifies the file descriptor's shared file offset. If another thread writes between `lseek()` and `write()`, a race condition occurs and data is written to the wrong location. `pwrite()` takes an explicit offset argument and writes atomically without modifying the file descriptor's offset."*

---

## 15. Pre-Submission Code Review Checklist

Before opening a Pull Request or merging code into `develop`, verify this checklist:

- [ ] **Clean Build**: `make clean && make` succeeds with **0 warnings** under `gcc -Wall -Wextra`.
- [ ] **No Hardcoded Values**: No hardcoded IP addresses (`127.0.0.1`), ports (`8080`), or absolute paths (`/home/user/...`).
- [ ] **Checked Return Values**: Every `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `open`, `read`, `write`, and `malloc` checks its return value.
- [ ] **Safe String Handling**: No `strcpy()`, `strcat()`, or unchecked `sprintf()`.
- [ ] **Memory Freed**: Every `malloc()` has a corresponding `free()`, especially in worker thread exit paths.
- [ ] **Sockets Closed**: Every opened socket descriptor is closed on both success and error branches.
- [ ] **Path Traversal Guarded**: Input topic names and filenames are validated against `/`, `\`, and `..`.
- [ ] **Signal Handled**: `signal(SIGPIPE, SIG_IGN)` is present at the start of `main()`.
- [ ] **Grader Compliant**: Executable names (`server`, `client`) and command-line arguments match the assignment specification exactly.

---

*This document is the authoritative coding reference for all members of Group 071 + 072. When in doubt during implementation, consult this guide or your peer reviewer.*
