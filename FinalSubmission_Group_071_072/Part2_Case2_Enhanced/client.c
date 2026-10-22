#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../common/protocol.h"
#include "../common/sha256_util.h"
#include "enhanced_protocol.h"

#define CLIENT_PATH_MAX 4096

typedef struct {
    char path[MAX_PATH_BUFFER_LEN];
    uint64_t size;
    uint8_t expected_sha256[SHA256_DIGEST_LENGTH];
} ClientFile;

typedef struct {
    char ip[128];
    char output_dir[MAX_PATH_BUFFER_LEN];
    uint16_t port;
    uint64_t session_id;
    uint32_t fingerprint;
    uint32_t file_count;
    uint32_t unit_count;
    ClientFile *files;
    int manifest_ready;
    pthread_mutex_t manifest_mutex;
} ClientSession;

typedef struct {
    ClientSession *session;
    int worker_id;
    int initial_fd;
} WorkerArgs;

typedef struct {
    int success;
    double elapsed_sec;
    uint64_t verified_bytes;
    double throughput_mib_s;
    double throughput_mb_s;
    int integrity_passed;
} TransferResult;

static int receive_expected(int fd, uint8_t type,
                            void *buffer, size_t capacity)
{
    FrameHeader header;

    if (recv_frame_header(fd, &header) != 0 ||
        header.type != type ||
        header.payload_len > capacity) {
        return -1;
    }

    return recv_frame_payload(fd, &header, buffer, capacity);
}

static int connect_server(ClientSession *session)
{
    int fd;
    struct sockaddr_in address;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(session->port);

    if (inet_pton(AF_INET, session->ip, &address.sin_addr) != 1 ||
        connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int make_directories(const char *path)
{
    char buffer[CLIENT_PATH_MAX];
    size_t length = strlen(path);
    size_t i;

    if (length == 0 || length >= sizeof(buffer)) {
        return -1;
    }

    memcpy(buffer, path, length + 1);

    for (i = 1; i < length; ++i) {
        if (buffer[i] == '/' || buffer[i] == '\\') {
            char saved = buffer[i];
            buffer[i] = '\0';

            if (buffer[0] != '\0' &&
                mkdir(buffer, 0777) != 0 && errno != EEXIST) {
                return -1;
            }

            buffer[i] = saved;
        }
    }

    if (mkdir(buffer, 0777) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

static int remove_directory_recursive(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *entry;
    char subpath[CLIENT_PATH_MAX];

    if (d == NULL) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }

    while ((entry = readdir(d)) != NULL) {
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
        if (lstat(subpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                remove_directory_recursive(subpath);
            } else {
                unlink(subpath);
            }
        }
    }

    closedir(d);
    return rmdir(path);
}

static int receive_manifest(int fd, ClientSession *session)
{
    EnhancedWorkPlanHeader header;
    uint32_t i;

    if (receive_expected(fd, MSG_WORK_PLAN,
                         &header, sizeof(header)) != 0) {
        return -1;
    }

    if (enhanced_ntoh64(header.session_id) != session->session_id ||
        enhanced_ntoh32(header.manifest_fingerprint) !=
            session->fingerprint) {
        fprintf(stderr, "Manifest/session mismatch\n");
        return -1;
    }

    session->file_count = enhanced_ntoh32(header.file_count);
    session->unit_count = enhanced_ntoh32(header.unit_count);

    if (session->file_count > 100000U) {
        return -1;
    }

    if (!session->manifest_ready) {
        session->files = calloc(
            session->file_count ? session->file_count : 1,
            sizeof(*session->files));

        if (session->files == NULL) {
            return -1;
        }
    }

    for (i = 0; i < session->file_count; ++i) {
        EnhancedFileMetadata metadata;
        char staging_path[CLIENT_PATH_MAX];
        char parent[CLIENT_PATH_MAX];
        char *slash;
        int output_fd;

        if (receive_expected(fd, MSG_FILE_BEGIN,
                             &metadata, sizeof(metadata)) != 0) {
            return -1;
        }

        if (enhanced_ntoh32(metadata.file_index) != i) {
            return -1;
        }

        metadata.filename[sizeof(metadata.filename) - 1] = '\0';
        metadata.relative_path[
            sizeof(metadata.relative_path) - 1] = '\0';

        if (metadata.relative_path[0] == '\0' ||
            strstr(metadata.relative_path, "..") != NULL) {
            fprintf(stderr, "Invalid relative file path\n");
            return -1;
        }

        if (session->manifest_ready) {
            if (strcmp(session->files[i].path,
                       metadata.relative_path) != 0 ||
                session->files[i].size !=
                    enhanced_ntoh64(metadata.file_size) ||
                memcmp(session->files[i].expected_sha256,
                       metadata.sha256, SHA256_DIGEST_LENGTH) != 0) {
                return -1;
            }
            continue;
        }

        snprintf(session->files[i].path,
                 sizeof(session->files[i].path), "%s",
                 metadata.relative_path);

        session->files[i].size =
            enhanced_ntoh64(metadata.file_size);
        memcpy(session->files[i].expected_sha256,
               metadata.sha256, SHA256_DIGEST_LENGTH);

        if (snprintf(staging_path, sizeof(staging_path), "%s/%s.part",
                     session->output_dir,
                     session->files[i].path) >= (int)sizeof(staging_path)) {
            return -1;
        }

        snprintf(parent, sizeof(parent), "%s", staging_path);
        slash = strrchr(parent, '/');

        if (slash != NULL) {
            *slash = '\0';

            if (make_directories(parent) != 0) {
                perror("Creating parent directory");
                return -1;
            }
        }

        output_fd = open(staging_path, O_CREAT | O_RDWR, 0666);

        if (output_fd < 0 ||
            ftruncate(output_fd, (off_t)session->files[i].size) != 0) {
            perror("Creating output file");
            if (output_fd >= 0) {
                close(output_fd);
            }
            return -1;
        }

        close(output_fd);
    }

    session->manifest_ready = 1;
    return 0;
}

static int send_worker_hello(int fd, ClientSession *session, int worker_id)
{
    EnhancedWorkerHello hello;

    hello.session_id = enhanced_hton64(session->session_id);
    hello.worker_id = enhanced_hton32((uint32_t)worker_id);

    return send_frame(fd, MSG_WORK_ASSIGN, 0,
                      &hello, sizeof(hello));
}

static int write_chunk(int fd, const unsigned char *data,
                       size_t length, uint64_t offset)
{
    size_t written = 0;

    while (written < length) {
        ssize_t result = pwrite(fd, data + written,
                                length - written,
                                (off_t)(offset + written));

        if (result < 0 && errno == EINTR) {
            continue;
        }

        if (result <= 0) {
            return -1;
        }

        written += (size_t)result;
    }

    return 0;
}

static int worker_stream(int fd, ClientSession *session, int worker_id)
{
    int manifest_result;

    pthread_mutex_lock(&session->manifest_mutex);
    manifest_result = receive_manifest(fd, session);
    pthread_mutex_unlock(&session->manifest_mutex);

    if (manifest_result != 0) {
        fprintf(stderr, "[worker %d] Manifest reception failed\n",
                worker_id);
        return -1;
    }

    for (;;) {
        FrameHeader header;

        if (recv_frame_header(fd, &header) != 0) {
            return -1;
        }

        if (header.type == MSG_WORK_END) {
            if (header.payload_len != 0) {
                void *discard = NULL;
                if (recv_frame_payload_alloc(fd, &header, &discard) != 0) {
                    free(discard);
                    return -1;
                }

                free(discard);
            }

            return 0;
        }

        if (header.type == MSG_ERROR) {
            char message[256] = {0};
            size_t length = header.payload_len;

            if (length >= sizeof(message)) {
                length = sizeof(message) - 1;
            }

            if (recv_frame_payload(fd, &header, message, length) != 0) {
                return -1;
            }

            fprintf(stderr, "[worker %d] Server error: %s\n",
                    worker_id, message);
            return -1;
        }

        if (header.type == MSG_WORK_ASSIGN) {
            EnhancedWorkAssignment assignment;

            if (header.payload_len != sizeof(assignment) ||
                recv_frame_payload(fd, &header, &assignment,
                                   sizeof(assignment)) != 0) {
                return -1;
            }
            continue;
        }

        if (header.type == MSG_WORK_DATA) {
            EnhancedWorkDataHeader data_header;
            unsigned char *payload;
            uint32_t unit_id;
            uint32_t file_index;
            uint32_t data_length;
            uint64_t file_offset;
            char output_path[CLIENT_PATH_MAX];
            int output_fd;
            EnhancedWorkAck ack;

            if (header.payload_len < sizeof(data_header) ||
                header.payload_len >
                    sizeof(data_header) + ENHANCED_DATA_CHUNK) {
                return -1;
            }

            payload = malloc((size_t)header.payload_len);

            if (payload == NULL) {
                return -1;
            }

            if (recv_frame_payload(fd, &header, payload,
                                   (size_t)header.payload_len) != 0) {
                free(payload);
                return -1;
            }
            memcpy(&data_header, payload, sizeof(data_header));

            unit_id = enhanced_ntoh32(data_header.unit_id);
            file_index = enhanced_ntoh32(data_header.file_index);
            file_offset = enhanced_ntoh64(data_header.file_offset);
            data_length = enhanced_ntoh32(data_header.data_length);

            if ((uint64_t)data_length !=
                    header.payload_len - sizeof(data_header)) {
                fprintf(stderr,
                        "[worker %d] Data length mismatch: declared=%u, actual=%llu\n",
                        worker_id, data_length,
                        (unsigned long long)(header.payload_len - sizeof(data_header)));
                free(payload);
                return -1;
            }
            if (file_index >= session->file_count) {
                fprintf(stderr,
                        "[worker %d] Invalid file index: %u (file count=%u)\n",
                        worker_id, file_index, session->file_count);
                free(payload);
                return -1;
            }
            if (file_offset > session->files[file_index].size ||
                data_length > session->files[file_index].size - file_offset) {
                fprintf(stderr,
                        "[worker %d] Invalid file range: index=%u offset=%llu length=%u size=%llu\n",
                        worker_id, file_index,
                        (unsigned long long)file_offset, data_length,
                        (unsigned long long)session->files[file_index].size);
                free(payload);
                return -1;
            }

            if (snprintf(output_path, sizeof(output_path), "%s/%s.part",
                         session->output_dir,
                         session->files[file_index].path) >=
                    (int)sizeof(output_path)) {
                free(payload);
                return -1;
            }

            output_fd = open(output_path, O_RDWR);

            if (output_fd < 0) {
                perror("Opening output staging file");
                free(payload);
                return -1;
            }
            if (write_chunk(output_fd,
                            payload + sizeof(data_header),
                            data_length, file_offset) != 0) {
                perror("Writing file chunk");
                close(output_fd);
                free(payload);
                return -1;
            }

            close(output_fd);
            free(payload);

            memset(&ack, 0, sizeof(ack));
            ack.session_id = enhanced_hton64(session->session_id);
            ack.unit_id = enhanced_hton32(unit_id);
            ack.file_index = enhanced_hton32(file_index);
            ack.committed_offset =
                enhanced_hton64(file_offset + data_length);

            if (send_frame(fd, MSG_CHUNK_ACK, 0,
                           &ack, sizeof(ack)) != 0) {
                return -1;
            }

            continue;
        }

        if (header.payload_len > 0) {
            void *discard = NULL;
            if (recv_frame_payload_alloc(fd, &header, &discard) != 0) {
                free(discard);
                return -1;
            }

            free(discard);
        }

        fprintf(stderr, "[worker %d] Unexpected frame type %u\n",
                worker_id, (unsigned)header.type);
        return -1;
    }
}

static void *worker_main(void *argument)
{
    WorkerArgs *args = (WorkerArgs *)argument;
    ClientSession *session = args->session;
    int worker_id = args->worker_id;
    int fd = args->initial_fd;

    free(args);

    for (;;) {
        int result;

        if (fd < 0) {
            fd = connect_server(session);

            if (fd < 0) {
                { struct timespec ts = {0, 50000000L}; nanosleep(&ts, NULL); }
                continue;
            }

            if (send_worker_hello(fd, session, worker_id) != 0) {
                close(fd);
                fd = -1;
                { struct timespec ts = {0, 50000000L}; nanosleep(&ts, NULL); }
                continue;
            }
        }

        result = worker_stream(fd, session, worker_id);
        close(fd);
        fd = -1;

        if (result == 0) {
            break;
        }

        fprintf(stderr,
                "[worker %d] Connection interrupted; reconnecting\n",
                worker_id);
        { struct timespec ts = {0, 50000000L}; nanosleep(&ts, NULL); }
    }

    return NULL;
}

static int verify_and_publish_files(ClientSession *session, int publish_staging, uint64_t *out_verified_bytes)
{
    uint32_t i;
    int all_passed = 1;
    uint64_t total_verified = 0;

    if (session == NULL || session->files == NULL) {
        return 0;
    }

    for (i = 0; i < session->file_count; ++i) {
        char check_path[CLIENT_PATH_MAX];
        char final_path[CLIENT_PATH_MAX];
        uint8_t actual_digest[SHA256_DIGEST_LENGTH];
        char expected_hex[SHA256_HEX_STRING_LENGTH];
        char computed_hex[SHA256_HEX_STRING_LENGTH];
        int hash_result;

        sha256_to_hex(session->files[i].expected_sha256, expected_hex);
        snprintf(final_path, sizeof(final_path), "%s/%s",
                 session->output_dir, session->files[i].path);

        if (publish_staging) {
            snprintf(check_path, sizeof(check_path), "%s/%s.part",
                     session->output_dir, session->files[i].path);
        } else {
            snprintf(check_path, sizeof(check_path), "%s", final_path);
        }

        hash_result = compute_file_sha256(check_path, actual_digest);
        if (hash_result != 0) {
            fprintf(stderr, "File '%s' [size %llu bytes]:\n",
                    session->files[i].path, (unsigned long long)session->files[i].size);
            fprintf(stderr, "  Expected SHA-256: %s\n", expected_hex);
            fprintf(stderr, "  Computed SHA-256: <UNREADABLE / MISSING FILE>\n");
            fprintf(stderr, "  SHA-256 verification: FAIL — file content does not match the expected digest\n");
            all_passed = 0;
            continue;
        }

        sha256_to_hex(actual_digest, computed_hex);

        if (sha256_equal(session->files[i].expected_sha256, actual_digest)) {
            printf("File '%s' [size %llu bytes]:\n",
                   session->files[i].path, (unsigned long long)session->files[i].size);
            printf("  Expected SHA-256: %s\n", expected_hex);
            printf("  Computed SHA-256: %s\n", computed_hex);
            printf("  SHA-256 verification: PASS\n");

            if (publish_staging) {
                if (rename(check_path, final_path) != 0) {
                    perror("Publishing verified file");
                    all_passed = 0;
                }
            }
            total_verified += session->files[i].size;
        } else {
            fprintf(stderr, "File '%s' [size %llu bytes]:\n",
                    session->files[i].path, (unsigned long long)session->files[i].size);
            fprintf(stderr, "  Expected SHA-256: %s\n", expected_hex);
            fprintf(stderr, "  Computed SHA-256: %s\n", computed_hex);
            fprintf(stderr, "  SHA-256 verification: FAIL — file content does not match the expected digest\n");
            all_passed = 0;
            if (publish_staging) {
                unlink(check_path);
            }
        }
    }

    if (out_verified_bytes != NULL) {
        *out_verified_bytes = total_verified;
    }

    return all_passed;
}

static int execute_transfer(const char *ip, uint16_t port, const char *topic,
                            const char *out_base_dir, int connections,
                            TransferResult *result)
{
    ClientSession session;
    MsgSessionStartPayload request;
    MsgSessionAcceptPayload accepted;
    pthread_t threads[ENHANCED_MAX_CONNECTIONS];
    int thread_count = 0;
    int first_fd;
    int i;
    struct timespec t_start, t_end;
    double elapsed;
    uint64_t verified_bytes = 0;
    int pass;

    memset(result, 0, sizeof(*result));
    memset(&session, 0, sizeof(session));

    if (strlen(ip) >= sizeof(session.ip) ||
        strlen(out_base_dir) + 1 + strlen(topic) >= sizeof(session.output_dir) ||
        strlen(topic) >= sizeof(request.topic)) {
        fprintf(stderr, "An argument is too long\n");
        return -1;
    }

    strcpy(session.ip, ip);
    snprintf(session.output_dir, sizeof(session.output_dir), "%s/%s", out_base_dir, topic);
    session.port = port;

    if (pthread_mutex_init(&session.manifest_mutex, NULL) != 0) {
        fprintf(stderr, "Could not initialize manifest mutex\n");
        return -1;
    }

    if (make_directories(session.output_dir) != 0) {
        perror("Creating output directory");
        pthread_mutex_destroy(&session.manifest_mutex);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t_start);

    first_fd = connect_server(&session);
    if (first_fd < 0) {
        perror("Connecting to server");
        pthread_mutex_destroy(&session.manifest_mutex);
        return -1;
    }

    memset(&request, 0, sizeof(request));
    strcpy(request.topic, topic);
    request.requested_conns = htonl((uint32_t)connections);

    if (send_frame(first_fd, MSG_SESSION_START, 0,
                   &request, sizeof(request)) != 0 ||
        receive_expected(first_fd, MSG_SESSION_ACCEPT,
                         &accepted, sizeof(accepted)) != 0) {
        fprintf(stderr, "Session initialization failed\n");
        close(first_fd);
        pthread_mutex_destroy(&session.manifest_mutex);
        return -1;
    }

    session.session_id = enhanced_ntoh64(accepted.session_id);
    session.fingerprint = enhanced_ntoh32(accepted.manifest_fingerprint);

    for (i = 0; i < connections; ++i) {
        WorkerArgs *args = malloc(sizeof(*args));

        if (args == NULL) {
            perror("Allocating worker arguments");
            break;
        }

        args->session = &session;
        args->worker_id = i;
        args->initial_fd = (i == 0) ? first_fd : -1;

        if (pthread_create(&threads[i], NULL,
                           worker_main, args) != 0) {
            fprintf(stderr, "Could not start worker %d\n", i);
            free(args);

            if (i == 0) {
                close(first_fd);
            }
            break;
        }

        ++thread_count;
    }

    for (i = 0; i < thread_count; ++i) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    elapsed = (double)(t_end.tv_sec - t_start.tv_sec) +
              (double)(t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    result->elapsed_sec = elapsed;

    if (thread_count != connections) {
        free(session.files);
        pthread_mutex_destroy(&session.manifest_mutex);
        return -1;
    }

    pass = verify_and_publish_files(&session, 1, &verified_bytes);
    result->verified_bytes = verified_bytes;
    result->integrity_passed = pass;
    result->success = pass;

    if (elapsed > 0.0) {
        result->throughput_mib_s = ((double)verified_bytes / (1024.0 * 1024.0)) / elapsed;
        result->throughput_mb_s  = ((double)verified_bytes / 1e6) / elapsed;
    }

    free(session.files);
    pthread_mutex_destroy(&session.manifest_mutex);

    return pass ? 0 : -1;
}

static int run_verify_mode(const char *ip, uint16_t port, const char *topic, const char *out_base_dir)
{
    ClientSession session;
    MsgSessionStartPayload request;
    MsgSessionAcceptPayload accepted;
    int fd;
    int pass;
    uint64_t verified_bytes = 0;

    memset(&session, 0, sizeof(session));
    strcpy(session.ip, ip);
    snprintf(session.output_dir, sizeof(session.output_dir), "%s/%s", out_base_dir, topic);
    session.port = port;

    if (pthread_mutex_init(&session.manifest_mutex, NULL) != 0) {
        return EXIT_FAILURE;
    }

    fd = connect_server(&session);
    if (fd < 0) {
        perror("Connecting to server for verification");
        pthread_mutex_destroy(&session.manifest_mutex);
        return EXIT_FAILURE;
    }

    memset(&request, 0, sizeof(request));
    strcpy(request.topic, topic);
    request.requested_conns = htonl(1);

    if (send_frame(fd, MSG_SESSION_START, 0,
                   &request, sizeof(request)) != 0 ||
        receive_expected(fd, MSG_SESSION_ACCEPT,
                         &accepted, sizeof(accepted)) != 0) {
        fprintf(stderr, "Session initialization failed during verification\n");
        close(fd);
        pthread_mutex_destroy(&session.manifest_mutex);
        return EXIT_FAILURE;
    }

    session.session_id = enhanced_ntoh64(accepted.session_id);
    session.fingerprint = enhanced_ntoh32(accepted.manifest_fingerprint);

    if (receive_manifest(fd, &session) != 0) {
        fprintf(stderr, "Failed to retrieve manifest for verification\n");
        close(fd);
        pthread_mutex_destroy(&session.manifest_mutex);
        return EXIT_FAILURE;
    }

    close(fd);

    printf("\n--- Standalone SHA-256 File Integrity Verification ---\n");
    printf("Topic: %s | Directory: %s | Files: %u\n", topic, session.output_dir, session.file_count);

    pass = verify_and_publish_files(&session, 0, &verified_bytes);

    printf("------------------------------------------------------\n");
    if (pass) {
        printf("Verification Result: ALL FILES MATCH (Verified %llu useful payload bytes)\n",
               (unsigned long long)verified_bytes);
    } else {
        fprintf(stderr, "Verification Result: INTEGRITY COMPROMISED (one or more mismatches)\n");
    }

    free(session.files);
    pthread_mutex_destroy(&session.manifest_mutex);

    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int run_auto_mode(const char *ip, uint16_t port, const char *topic, const char *out_base_dir)
{
    const int candidates[] = {1, 2, 4, 8};
    const int num_candidates = (int)(sizeof(candidates) / sizeof(candidates[0]));
    TransferResult results[4];
    char final_topic_dir[CLIENT_PATH_MAX];
    int best_idx = -1;
    double best_throughput = -1.0;
    int i;

    snprintf(final_topic_dir, sizeof(final_topic_dir), "%s/%s", out_base_dir, topic);

    printf("\n======================================================================\n");
    printf("Adaptive Parallel TCP Transfer Auto-Selection\n");
    printf("Topic: %s | Server: %s:%u\n", topic, ip, port);
    printf("Evaluating candidate connection counts: 1, 2, 4, 8\n");
    printf("======================================================================\n\n");

    for (i = 0; i < num_candidates; ++i) {
        int conns = candidates[i];
        char trial_dir[CLIENT_PATH_MAX];
        int ret;

        snprintf(trial_dir, sizeof(trial_dir), "%s/.trial_%d", out_base_dir, conns);
        remove_directory_recursive(trial_dir);

        printf("Adaptive transfer: testing %d connection%s...\n",
               conns, (conns == 1) ? "" : "s");

        ret = execute_transfer(ip, port, topic, trial_dir, conns, &results[i]);

        if (ret == 0 && results[i].integrity_passed) {
            printf("Adaptive transfer: trial with %d connection%s: duration = %.4f s, payload = %llu bytes, throughput = %.2f MiB/s (%.2f MB/s) [PASS]\n\n",
                   conns, (conns == 1) ? "" : "s",
                   results[i].elapsed_sec,
                   (unsigned long long)results[i].verified_bytes,
                   results[i].throughput_mib_s,
                   results[i].throughput_mb_s);
        } else {
            fprintf(stderr, "Adaptive transfer: trial with %d connection%s: FAILED [EXCLUDED]\n\n",
                    conns, (conns == 1) ? "" : "s");
            results[i].success = 0;
            results[i].integrity_passed = 0;
            results[i].throughput_mib_s = -1.0;
        }

        /* Cooldown pause between trials so TCP sockets close cleanly */
        {
            struct timespec pause = {0, 50000000L}; /* 50ms */
            nanosleep(&pause, NULL);
        }
    }

    /*
     * Selection rule:
     * Choose the candidate with highest useful throughput.
     * Tie-breaking rule: Within a 2% margin, select the candidate with FEWER
     * connections to minimize thread context switching and socket overhead.
     */
    for (i = 0; i < num_candidates; ++i) {
        if (results[i].success && results[i].integrity_passed) {
            if (best_idx == -1 || results[i].throughput_mib_s > best_throughput * 1.02) {
                best_throughput = results[i].throughput_mib_s;
                best_idx = i;
            }
        }
    }

    if (best_idx < 0) {
        fprintf(stderr, "Adaptive transfer: All candidate trials failed. No valid configuration.\n");
        return EXIT_FAILURE;
    }

    /* Publish the winning trial's files to final output directory */
    {
        char winning_topic_dir[CLIENT_PATH_MAX];
        snprintf(winning_topic_dir, sizeof(winning_topic_dir), "%s/.trial_%d/%s",
                 out_base_dir, candidates[best_idx], topic);

        remove_directory_recursive(final_topic_dir);
        if (rename(winning_topic_dir, final_topic_dir) != 0) {
            perror("Promoting winning trial directory");
        }
    }

    /* Clean up all temporary trial directories */
    for (i = 0; i < num_candidates; ++i) {
        char trial_dir[CLIENT_PATH_MAX];
        snprintf(trial_dir, sizeof(trial_dir), "%s/.trial_%d", out_base_dir, candidates[i]);
        remove_directory_recursive(trial_dir);
    }

    printf("======================================================================\n");
    printf("Adaptive Parallel TCP Transfer Benchmark Summary:\n");
    for (i = 0; i < num_candidates; ++i) {
        if (results[i].success) {
            printf("  %d connection%s : %8.2f MiB/s (%6.2f MB/s) in %6.4f s [PASS]\n",
                   candidates[i], (candidates[i] == 1) ? " " : "s",
                   results[i].throughput_mib_s, results[i].throughput_mb_s,
                   results[i].elapsed_sec);
        } else {
            printf("  %d connection%s : FAILED [EXCLUDED]\n",
                   candidates[i], (candidates[i] == 1) ? " " : "s");
        }
    }
    printf("----------------------------------------------------------------------\n");
    printf("Selected configuration: %d connection%s\n",
           candidates[best_idx], (candidates[best_idx] == 1) ? "" : "s");
    printf("Measured Peak Throughput: %.2f MiB/s (%.2f MB/s)\n",
           results[best_idx].throughput_mib_s, results[best_idx].throughput_mb_s);
    printf("Selection rule: Highest verified useful throughput (tie-breaker: fewer connections)\n");
    printf("Verified final payload available at: %s\n", final_topic_dir);
    printf("======================================================================\n");

    return EXIT_SUCCESS;
}

static int parse_port(const char *text, uint16_t *port)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);

    if (errno != 0 || text[0] == '\0' || *end != '\0' ||
        value < 1 || value > 65535) {
        return -1;
    }

    *port = (uint16_t)value;
    return 0;
}

int main(int argc, char **argv)
{
    uint16_t port;
    long connections = 1;
    char *end;
    TransferResult res;
    int ret;

    if (argc != 5 && argc != 6) {
        fprintf(stderr,
                "Usage: %s <server_ip> <server_port> <topic> "
                "<output_dir> [num_connections | --auto | --verify]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_port(argv[2], &port) != 0) {
        fprintf(stderr, "Invalid server port\n");
        return EXIT_FAILURE;
    }

    if (argc == 6) {
        if (strcmp(argv[5], "--auto") == 0) {
            return run_auto_mode(argv[1], port, argv[3], argv[4]);
        }
        if (strcmp(argv[5], "--verify") == 0) {
            return run_verify_mode(argv[1], port, argv[3], argv[4]);
        }

        errno = 0;
        connections = strtol(argv[5], &end, 10);

        if (errno != 0 || argv[5][0] == '\0' || *end != '\0' ||
            connections < 1 ||
            connections > ENHANCED_MAX_CONNECTIONS) {
            fprintf(stderr, "Invalid number of connections (1-%d) or option (--auto, --verify)\n",
                    ENHANCED_MAX_CONNECTIONS);
            return EXIT_FAILURE;
        }
    }

    /* Explicit connection-count mode (default 1 connection, or explicit 1..16) */
    ret = execute_transfer(argv[1], port, argv[3], argv[4], (int)connections, &res);
    if (ret != 0 || !res.integrity_passed) {
        fprintf(stderr, "Transfer failed or file integrity verification failed.\n");
        return EXIT_FAILURE;
    }

    printf("Client finished successfully. Verified %llu bytes in %.4f s (%.2f MiB/s, %.2f MB/s).\n",
           (unsigned long long)res.verified_bytes, res.elapsed_sec,
           res.throughput_mib_s, res.throughput_mb_s);

    return EXIT_SUCCESS;
}
