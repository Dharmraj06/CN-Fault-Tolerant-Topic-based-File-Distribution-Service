# IT305 Computer Networks: College-Level C Systems Coding Standards & Viva Guide

**Project:** Fault-Tolerant Topic-Based File Distribution Service  
**Course:** IT305 Computer Networks Lab (Autumn 2026), DA-IICT  
**Team:** Merged Group 071 + Group 072  
**Document Status:** Official Implementation Style & Systems Engineering Standard  
**Authority:** Subordinate to [PART2_TASKS_ROADMAP.md](../../PART2_TASKS_ROADMAP.md)  

---

## 1. Core Coding Philosophy

This coding standard is tailored specifically for a second- or third-year undergraduate Computer Networks systems project. The goal is to produce **clean, straightforward, robust, and understandable POSIX C code that demonstrates authentic mastery of sockets, file I/O, multi-threading, and fault recovery**.

### 1.1 Authentic Student-Level Excellence
- **Simplicity Over Cleverness:** Write code that any team member can immediately understand and confidently defend during a rigorous faculty viva or lab evaluation.
- **Explicit Control Flow:** Prefer clear `if-else` blocks, deterministic state machines, and structured loops over dense pointer arithmetic, cryptic macros, or complex design patterns.
- **No Overengineering:** Do not implement custom thread pools, dynamic plugin loaders, complex hashing libraries, or asynchronous event loops unless explicitly required by the approved design document.
- **Zero Tolerance for Sloppiness:** "Student-level" does **NOT** mean buggy, un-checked, or poorly written code. Full return-value checking, memory leak prevention, strict thread safety, and bit-exact protocol adherence are 100% mandatory.
- **Viva-Ready Design:** Every non-trivial data structure, synchronization lock, and protocol header must have an obvious technical justification that aligns directly with network protocol principles taught in IT305.

---

## 2. Appropriate C Language Standards & Portability

- **Compiler Standard:** Target **C99 / POSIX.1-2008** using GCC:
  ```bash
  gcc -std=c99 -Wall -Wextra -Werror -pedantic -pthread
  ```
- **Standard Library Headers:** Rely strictly on standard POSIX and ISO C headers:
  - `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<stdint.h>`, `<stdbool.h>`, `<errno.h>`
  - Sockets & Networking: `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<endian.h>`
  - File I/O & Filesystem: `<fcntl.h>`, `<unistd.h>`, `<sys/stat.h>`, `<dirent.h>`
  - Concurrency & Timing: `<pthread.h>`, `<time.h>`
- **No C++ Constructs:** The code is 100% pure C. Never use `class`, `new`, `delete`, references (`&`), templates, or C++ standard libraries (`<iostream>`, `std::vector`).
- **No External Dependencies:** External networking frameworks (e.g., `libcurl`, `libevent`, `Boost.Asio`) or serialization libraries (`cJSON`, `protobuf`) are strictly forbidden. The wire protocol is implemented directly over raw TCP sockets.

---

## 3. Naming Conventions

Consistency across all six team members is essential. Follow these simple naming rules across all modules:

| Code Element | Naming Convention | Examples |
|---|---|---|
| **Variables & Parameters** | `snake_case` | `client_fd`, `session_id`, `bytes_read`, `file_offset` |
| **Functions** | `snake_case` with module prefix | `proto_send_frame()`, `topic_scan()`, `sess_lookup()` |
| **Structures & Typedefs** | `PascalCase` | `FrameHeader`, `SessionState`, `TopicManifest`, `WorkUnit` |
| **Enums & Constants** | `UPPER_SNAKE_CASE` | `MSG_FILE_DATA`, `DEFAULT_CHUNK_SIZE`, `SESSION_ACTIVE` |
| **Macros & Header Guards** | `UPPER_SNAKE_CASE` | `PROTOCOL_H`, `MAX_TOPIC_LEN`, `CHECK_STATUS()` |
| **File Names** | `snake_case.[c\|h]` | `protocol.c`, `fault_injection.h`, `file_transfer.c` |

---

## 4. Function Design & Structural Invariants

1. **Focused Single Responsibility:** A function should perform one clear task. Keep functions typically between 30 and 80 lines of code. Avoid massive 500-line functions that mix socket parsing, disk I/O, and session state.
2. **Explicit Parameter Contracts:**
   - Input buffers: `const void *buf` or `const char *str`.
   - Output buffers: `void *out_buf, size_t max_len`.
   - Pointer arguments must be checked for `NULL` at the top of the function.
3. **Consistent Return Code Semantics:**
   - Functions returning status: return `0` on success, negative integer (e.g., `-1`) on error.
   - Lookup functions returning pointers: return `pointer` on success, `NULL` if not found or on allocation error.
   - Socket I/O functions: return `0` on success, `-1` on socket error, `1` on remote EOF/closure.
4. **Clean Exit & Resource Cleanup (`goto cleanup` Pattern):**
   In functions allocating multiple resources (e.g., socket, opened file, heap memory), use a structured forward `goto cleanup` to prevent resource leaks:
   ```c
   int process_file_transfer(const char *path, int sock_fd) {
       int file_fd = -1;
       char *buffer = NULL;
       int ret = -1;

       file_fd = open(path, O_RDONLY);
       if (file_fd < 0) {
           perror("open failed");
           goto cleanup;
       }

       buffer = malloc(DEFAULT_CHUNK_SIZE);
       if (!buffer) {
           perror("malloc failed");
           goto cleanup;
       }

       /* ... Perform streaming ... */
       ret = 0; /* Success */

   cleanup:
       if (buffer) free(buffer);
       if (file_fd >= 0) close(file_fd);
       return ret;
   }
   ```

---

## 5. Comments, Documentation & Viva Preparation

Comments must provide valuable context rather than echoing the syntax:
- **Explain the "Why":** Comment on protocol boundary decisions, locking rules, and error recovery policies.
- **Document Mutex Invariants:** Explicitly state which lock protects which struct fields and which operations are permitted while holding a lock.
- **Viva Tips in Comments:** Where an undergraduate student is expected to explain a systems call during evaluation, briefly document the concept:
  ```c
  /*
   * VIVA NOTE: TCP is a byte-stream transport without inherent frame boundaries.
   * We loop recv() until exactly 16 bytes (FrameHeader) are collected to prevent
   * partial-read corruption caused by TCP segment fragmentation.
   */
  ```

---

## 6. Comprehensive Error Handling & Defensive Programming

Every systems call interacts with the kernel and can fail. Check every return value explicitly:

### 6.1 Sockets & Networking
```c
int sockfd = socket(AF_INET, SOCK_STREAM, 0);
if (sockfd < 0) {
    perror("socket creation failed");
    return -1;
}

int opt = 1;
if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt SO_REUSEADDR failed");
    close(sockfd);
    return -1;
}
```

### 6.2 Partial Sends & Receives
Never assume a single `send()` or `recv()` call moves all requested bytes:
```c
/* Standard loop for complete transmission */
int send_all(int fd, const void *buf, size_t len) {
    const char *ptr = (const char *)buf;
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t n = send(fd, ptr + total_sent, len - total_sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue; /* Retry on signal interrupt */
            return -1;
        }
        if (n == 0) return -1; /* Connection terminated prematurely */
        total_sent += (size_t)n;
    }
    return 0;
}
```

### 6.3 Heap Allocations
Always verify `malloc()` return pointers:
```c
TopicManifest *manifest = malloc(sizeof(TopicManifest));
if (!manifest) {
    fprintf(stderr, "FATAL: Out of memory allocating TopicManifest\n");
    return -1;
}
```

---

## 7. Memory Management & Pointer Discipline

1. **Clear Allocation Ownership:** If a function allocates heap memory and returns it (e.g., `build_topic_manifest()`), the header must document that the caller owns the memory and is responsible for invoking the corresponding cleanup routine (`free_topic_manifest()`).
2. **Zero-Out Buffers:** Initialize structs and heap buffers immediately to prevent garbage memory bugs:
   ```c
   memset(&header, 0, sizeof(header));
   ```
3. **No Dangling Pointers:** Set pointers to `NULL` immediately after freeing:
   ```c
   free(ptr);
   ptr = NULL;
   ```
4. **Buffer Size Invariants:** When copying strings (such as filenames or topics), strictly enforce bounded operations (`snprintf`, `strncpy` with explicit null termination):
   ```c
   snprintf(dest_path, sizeof(dest_path), "%s/%s", root_dir, topic);
   dest_path[sizeof(dest_path) - 1] = '\0';
   ```

---

## 8. TCP Socket Programming Conventions

1. **Stream Protocol Semantics:** Treat TCP as an unstructured stream of bytes. Frame boundaries must be recovered using the 16-byte `FrameHeader`.
2. **Network Byte Order (Endianness):** Multi-byte integers sent over the wire must be converted using network byte order:
   - 16-bit: `htons()` / `ntohs()`
   - 32-bit: `htonl()` / `ntohl()`
   - 64-bit: `htobe64()` / `be64toh()` from `<endian.h>`
3. **Header Validation Before Allocation:** Never trust incoming network frames blindly. Verify `magic == 0x5446` and `version == 0x01`. Reject frames whose `payload_len` exceeds safe maximum limits (e.g., $> 10$ MB) to prevent malicious denial-of-service memory exhaustion.
4. **Graceful Socket Shutdown:** Close socket descriptors cleanly in both normal and error termination paths using `close(fd)`.

---

## 9. File-Transfer & Disk I/O Conventions

1. **Bounded Streaming (Never Buffer Full Files in RAM):**
   The `animals10` dataset contains multi-megabyte images. Reading entire files into memory with a massive `malloc()` is strictly prohibited. Files must be read and written in bounded chunks (e.g., `DEFAULT_CHUNK_SIZE = 64 * 1024` / 64 KB).
2. **Binary Safety:**
   Files being transferred are binary JPEG and PNG images. **Never use string functions (`strlen`, `strcpy`, `strcat`, `sprintf`) on raw file chunk payloads.** File data may contain null bytes (`0x00`) anywhere.
3. **Disk Flushing (`fsync`):**
   Before transmitting a `CHUNK_ACK` in Part 2 Case 2, call `sync_file_to_disk()` (`fsync(fd)` or `fdatasync(fd)`) to guarantee the chunk is committed to disk before acknowledging.
4. **Offset Synchronization (`ftruncate` & `lseek`):**
   When reconnecting in Case 2, if the client destination file has unacknowledged bytes, call `ftruncate(fd, authoritative_offset)` followed by `lseek(fd, authoritative_offset, SEEK_SET)`.

---

## 10. Concurrency & POSIX Mutex Discipline

1. **Multi-Threaded Server Model:**
   The multi-threaded server creates one detached POSIX worker thread per accepted client connection:
   ```c
   pthread_t tid;
   if (pthread_create(&tid, NULL, worker_thread, client_ctx) != 0) {
       perror("pthread_create failed");
       close(client_sock);
   } else {
       pthread_detach(tid); /* Automatically reclaim thread resources upon exit */
   }
   ```
2. **Strict Mutex Invariants:**
   - Protect global shared structures (such as the Case 2 `SessionState` table and Case 2 Enhanced `WorkPlan`) with dedicated `pthread_mutex_t` locks.
   - **THE GOLDEN CONCURRENCY RULE:** **Never hold a mutex while performing network socket I/O (`send()`, `recv()`) or disk I/O (`read()`, `write()`).**
   - Acquire the mutex, perform the in-memory lookup or state update (which takes microseconds), release the mutex, and only then transmit the network packet.
3. **Thread-Safe PRNG:**
   Do **NOT** use standard `rand()` across multiple worker threads because it uses shared global state. Use thread-safe `rand_r(&seed)` with a per-worker seed for probabilistic fault injection.

---

## 11. Fault Injection & Recovery Discipline

1. **Deterministic PRNG Initialization:**
   Initialize fault injection using an explicit seed:
   ```c
   int init_fault_injection(double failure_prob, unsigned int seed);
   ```
2. **Decision Point Placement:**
   As specified in Tier 2 Key (p. 4), the failure decision is evaluated after a chunk is ready and immediately before sending:
   ```c
   if (should_inject_fault()) {
       execute_fault_disconnect(client_sock, "Intentional probabilistic drop");
       pthread_exit(NULL);
   }
   ```
3. **Abrupt Disconnect Simulation:**
   `execute_fault_disconnect()` must immediately close the active socket descriptor without transmitting graceful `CLOSE` or `ERROR` frames, accurately simulating an unannounced network failure.

---

## 12. Session Management & Checkpoint Invariants

1. **Monotonic Progress:**
   In Part 2 Case 2, server checkpoint offsets are strictly non-decreasing:
   $$0 \le \text{current\_file\_offset} \le \text{current\_file\_size}$$
2. **Authoritative Server Principle:**
   The server is the sole source of truth for committed progress. A client-proposed offset during `SESSION_RESUME` is a proposal; the server validates it against its session table and returns the authoritative offset.
3. **Topic & Manifest Validation:**
   Every session tracks a 32-bit `manifest_fingerprint`. If the underlying topic files on the server are modified or deleted between disconnect and resume, the fingerprint mismatches, and the resume is rejected with `RESUME_ERROR`.

---

## 13. Case 2 Enhanced Parallel Transfer & Positional Writes

1. **Interval Disjointness:**
   Work units must partition the topic into non-overlapping byte ranges:
   $$\forall i \ne j, \quad [\text{start}_i, \text{end}_i) \cap [\text{start}_j, \text{end}_j) = \emptyset$$
2. **Atomic Positional `pwrite()`:**
   Multiple client threads write concurrently into the same destination file using:
   ```c
   pwrite(fd, buf, count, offset);
   ```
   POSIX `pwrite()` is atomic and position-independent. Because byte ranges are non-overlapping, threads never overwrite each other's data and require zero file-pointer locking.
3. **Single-Worker Fault Isolation:**
   If worker connection $K$ drops, only unit $U_K$ is marked failed and re-queued. All other parallel connections continue streaming without interruption.

---

## 14. Constants & Configuration Management

1. **No Magic Numbers:** Define all limits and defaults as named constants in headers:
   ```c
   #define PROTOCOL_MAGIC        0x5446
   #define DEFAULT_CHUNK_SIZE    (64 * 1024)
   #define MAX_TOPIC_LEN         128
   #define MAX_FILENAME_LEN      256
   #define SESSION_TTL_SECONDS   300
   ```
2. **Zero Hardcoded Paths or Ports:** All network addresses, ports, and directory paths must be parsed dynamically from command-line arguments in accordance with Tier 1 specifications.

---

## 15. Makefiles & Compiler Cleanliness

- All Makefiles must compile with `-Wall -Wextra -Werror -pedantic -pthread`.
- Running `make` must build the binaries with zero warnings.
- Running `make clean` must remove all compiled object files (`*.o`) and binaries (`server`, `client`).

---

## 16. Testing Expectations & Objective Verification

1. **Every Phase Tests Itself:** Unit tests and loopback integration tests must be written and executed within the phase that implements the feature.
2. **Bit-Exact Verification:** Every file download test must be validated using `diff -r`:
   ```bash
   diff -r ./dataset/cane ./downloads/cane
   ```
   The test passes only if `diff -r` produces **zero output**.
3. **Valgrind Verification:** Run executables under Valgrind to ensure zero memory leaks:
   ```bash
   valgrind --leak-check=full ./server 9001 ./dataset
   ```

---

## 17. Common Code Smells & Practices to Avoid

The following anti-patterns are strictly prohibited:

- ❌ **Using `strlen()` on Binary Payloads:** Will corrupt file data upon encountering a `0x00` byte.
- ❌ **Calling `read()` or `recv()` Once:** Assuming an entire packet arrives in one call causes race conditions.
- ❌ **Holding a Mutex Across Network Calls:** Causes catastrophic thread serialization and deadlocks.
- ❌ **Hardcoding Port Numbers or File Paths:** Immediately fails auto-grading scripts.
- ❌ **Buffering Whole Files in Memory:** Causes out-of-memory crashes on large files.
- ❌ **Silencing Compiler Warnings:** Hiding bugs behind `-Wno-...` flags is unacceptable.
- ❌ **Global Unprotected Variables:** Mutable state accessed by multiple worker threads must be protected by a mutex.

---

## 18. Peer Code Review Checklist

Before any Pull Request is approved and merged into `develop`, the assigned peer reviewer must verify this 13-point checklist:

- [ ] **1. Grader CLI Compliance:** Matches mandatory CLI argument format exactly.
- [ ] **2. Pure C99:** Zero C++ constructs or non-POSIX dependencies.
- [ ] **3. Compiler Cleanliness:** Compiles with `-Wall -Wextra -Werror -pedantic` with zero warnings.
- [ ] **4. Error Checking:** Return values of all `socket`, `bind`, `open`, `read`, `write`, and `malloc` calls checked.
- [ ] **5. Partial I/O Loops:** Socket reads and writes use `send_all()` and `recv_all()`.
- [ ] **6. Framing Adherence:** Uses 16-byte fixed binary header with network byte order.
- [ ] **7. Mutex Discipline:** No locks held during network `send`/`recv` or disk I/O.
- [ ] **8. Resource Cleanup:** All file descriptors, sockets, and allocated memory freed on error paths.
- [ ] **9. Binary Safety:** No string functions used on binary payload buffers.
- [ ] **10. Bounded Streaming:** File transfers stream in 64 KB chunks without loading full files into RAM.
- [ ] **11. Data Integrity:** Verified via bit-exact `diff -r` matching the source topic directory.
- [ ] **12. Unit & Integration Tests:** Required test scripts present, executed, and logs verified.
- [ ] **13. Viva Understandability:** Author can clearly explain and defend all algorithms and systems calls.
