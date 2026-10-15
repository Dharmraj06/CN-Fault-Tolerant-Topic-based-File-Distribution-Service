
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "../common/protocol.h"
#include "../common/topic_manager.h"
#include "../common/fault_injection.h"
#include "../common/session.h"
#include "../common/work_plan.h"
#include "enhanced_protocol.h"

#define SERVER_BACKLOG 32
#define SERVER_CHUNK_SIZE ENHANCED_DATA_CHUNK

typedef struct {
    int fd;
    int worker_id;
} WorkerArgs;

static char g_topics_root[MAX_PATH_BUFFER_LEN];
static TopicManifest *g_manifest;
static WorkPlan *g_plan;
static uint64_t g_session_id;
static int g_max_workers;
static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static int receive_payload(int fd, uint8_t expected_type,
                           void *buffer, size_t capacity)
{
    FrameHeader header;

    if (recv_frame_header(fd, &header) != 0 ||
        header.type != expected_type ||
        header.payload_len > capacity) {
        return -1;
    }

    return recv_frame_payload(fd, &header, buffer, capacity);
}

static int send_error_frame(int fd, const char *message)
{
    size_t length = strlen(message) + 1;

    if (length > 255) {
        length = 255;
    }

    return send_frame(fd, MSG_ERROR, 0, message, length);
}

static int send_manifest(int fd)
{
    EnhancedWorkPlanHeader plan_header;
    uint32_t i;

    memset(&plan_header, 0, sizeof(plan_header));
    plan_header.session_id = enhanced_hton64(g_session_id);
    plan_header.file_count = enhanced_hton32(g_manifest->file_count);
    plan_header.unit_count = enhanced_hton32(g_plan->total_units);
    plan_header.manifest_fingerprint =
        enhanced_hton32(g_manifest->manifest_fingerprint);

    if (send_frame(fd, MSG_WORK_PLAN, 0,
                   &plan_header, sizeof(plan_header)) != 0) {
        return -1;
    }

    for (i = 0; i < g_manifest->file_count; ++i) {
        EnhancedFileMetadata metadata;

        memset(&metadata, 0, sizeof(metadata));
        metadata.file_index = enhanced_hton32(i);
        metadata.file_size =
            enhanced_hton64(g_manifest->files[i].file_size);

        snprintf(metadata.filename, sizeof(metadata.filename), "%s",
                 g_manifest->files[i].filename);
        snprintf(metadata.relative_path, sizeof(metadata.relative_path),
                 "%s", g_manifest->files[i].relative_path);

        if (send_frame(fd, MSG_FILE_BEGIN, 0,
                       &metadata, sizeof(metadata)) != 0) {
            return -1;
        }
    }

    return 0;
}

static int session_is_complete(void)
{
    int complete;

    pthread_mutex_lock(&g_plan->plan_mutex);
    complete = (g_plan->completed_units == g_plan->total_units);
    pthread_mutex_unlock(&g_plan->plan_mutex);

    return complete;
}

static void *worker_main(void *argument)
{
    WorkerArgs *args = (WorkerArgs *)argument;
    int fd = args->fd;
    int worker_id = args->worker_id;
    int active_unit = -1;

    free(args);

    fprintf(stderr, "[worker %d] connected\n", worker_id);

    if (send_manifest(fd) != 0) {
        close(fd);
        return NULL;
    }

    for (;;) {
        WorkUnit unit;
        int claim_result = claim_next_work_unit(
            g_plan, worker_id, &unit);

        if (claim_result < 0) {
            break;
        }

        if (claim_result == 0) {
            if (session_is_complete()) {
                (void)send_frame(fd, MSG_WORK_END, 0, NULL, 0);
                break;
            }

            {
                struct timespec pause_time;
                pause_time.tv_sec = 0;
                pause_time.tv_nsec = 10000000L;
                nanosleep(&pause_time, NULL);
            }
            continue;
        }

        active_unit = (int)unit.unit_id;

        {
            EnhancedWorkAssignment assignment;

            assignment.unit_id = enhanced_hton32(unit.unit_id);
            assignment.file_index = enhanced_hton32(unit.file_index);
            assignment.start_offset =
                enhanced_hton64(unit.start_offset);
            assignment.end_offset =
                enhanced_hton64(unit.end_offset);
            assignment.committed_offset =
                enhanced_hton64(unit.committed_offset);

            if (send_frame(fd, MSG_WORK_ASSIGN, 0,
                           &assignment, sizeof(assignment)) != 0) {
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                break;
            }
        }

        while (unit.committed_offset < unit.end_offset) {
            uint64_t remaining = unit.end_offset - unit.committed_offset;
            size_t amount = remaining < SERVER_CHUNK_SIZE
                          ? (size_t)remaining : SERVER_CHUNK_SIZE;
            char path[MAX_PATH_BUFFER_LEN * 2];
            unsigned char *payload;
            EnhancedWorkDataHeader data_header;
            FILE *source;
            size_t bytes_read;
            FrameHeader response_header;
            EnhancedWorkAck ack;
            uint64_t expected_offset;

            if (should_inject_fault()) {
                char description[256];

                snprintf(description, sizeof(description),
                         "worker=%d unit=%u offset=%llu",
                         worker_id, unit.unit_id,
                         (unsigned long long)unit.committed_offset);

                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                execute_fault_disconnect(fd, description);
                return NULL;
            }

            if (snprintf(path, sizeof(path), "%s/%s",
                         g_manifest->directory_path,
                         unit.relative_path) >= (int)sizeof(path)) {
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }

            source = fopen(path, "rb");
            if (source == NULL) {
                fprintf(stderr, "[worker %d] fopen failed for '%s': %s\\n",
                        worker_id, path, strerror(errno));
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }
            if (fseeko(source, (off_t)unit.committed_offset, SEEK_SET) != 0) {
                fprintf(stderr, "[worker %d] fseeko failed for '%s' at offset %llu: %s\\n",
                        worker_id, path,
                        (unsigned long long)unit.committed_offset, strerror(errno));
                fclose(source);
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }

            payload = malloc(sizeof(data_header) + amount);
            if (payload == NULL) {
                fclose(source);
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }

            bytes_read = fread(payload + sizeof(data_header), 1, amount, source);
            fclose(source);

            if (bytes_read != amount) {
                fprintf(stderr,
                        "[worker %d] Read failed for '%s': expected %zu bytes, read %zu\\n",
                        worker_id, path, amount, bytes_read);
                free(payload);
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }

            data_header.unit_id = enhanced_hton32(unit.unit_id);
            data_header.file_index = enhanced_hton32(unit.file_index);
            data_header.file_offset =
                enhanced_hton64(unit.committed_offset);
            data_header.data_length = enhanced_hton32((uint32_t)amount);
            memcpy(payload, &data_header, sizeof(data_header));

            if (send_frame(fd, MSG_WORK_DATA, 0, payload,
                           sizeof(data_header) + amount) != 0) {
                free(payload);
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                break;
            }

            free(payload);

            if (recv_frame_header(fd, &response_header) != 0) {
                fprintf(stderr, "[worker %d] ACK header receive failed\n", worker_id);
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }
            if (response_header.type != MSG_CHUNK_ACK) {
                fprintf(stderr, "[worker %d] Expected ACK type %d, received %d\n",
                        worker_id, MSG_CHUNK_ACK, response_header.type);
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }
            if (response_header.payload_len != sizeof(ack)) {
               fprintf(stderr,
        "[worker %d] ACK payload size mismatch: expected %zu, received %llu\n",
        worker_id, sizeof(ack),
        (unsigned long long)response_header.payload_len);              (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }
            if (recv_frame_payload(fd, &response_header,
                                   &ack, sizeof(ack)) != 0) {
                fprintf(stderr, "[worker %d] ACK payload receive failed\n", worker_id);
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }

            ack.session_id = enhanced_ntoh64(ack.session_id);
            ack.unit_id = enhanced_ntoh32(ack.unit_id);
            ack.file_index = enhanced_ntoh32(ack.file_index);
            ack.committed_offset =
                enhanced_ntoh64(ack.committed_offset);

            expected_offset = unit.committed_offset + amount;

            if (ack.session_id != g_session_id ||
                ack.unit_id != unit.unit_id ||
                ack.file_index != unit.file_index ||
                ack.committed_offset != expected_offset ||
                mark_work_unit_progress(g_plan, unit.unit_id,
                                        worker_id, expected_offset) != 0) {
                (void)mark_work_unit_failed(g_plan, unit.unit_id);
                close(fd);
                return NULL;
            }

            unit.committed_offset = expected_offset;
        }

        if (unit.committed_offset == unit.end_offset) {
            if (mark_work_unit_complete(g_plan, unit.unit_id) != 0) {
                close(fd);
                return NULL;
            }

            active_unit = -1;
        }

        (void)active_unit;
    }

    close(fd);
    fprintf(stderr, "[worker %d] disconnected\n", worker_id);
    return NULL;
}

static int create_listener(uint16_t port)
{
    int fd;
    int reuse = 1;
    struct sockaddr_in address;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                     &reuse, sizeof(reuse));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(fd, SERVER_BACKLOG) != 0) {
        perror("bind/listen");
        close(fd);
        return -1;
    }

    return fd;
}

static int initialize_session(int fd)
{
    MsgSessionStartPayload request;
    MsgSessionAcceptPayload accepted;
    uint32_t requested;
    TopicError topic_error;
    WorkPlan *new_plan = NULL;

    if (receive_payload(fd, MSG_SESSION_START,
                        &request, sizeof(request)) != 0) {
        return -1;
    }

    request.topic[sizeof(request.topic) - 1] = '\0';
    requested = ntohl(request.requested_conns);

    if (requested < 1 || requested > ENHANCED_MAX_CONNECTIONS) {
        (void)send_error_frame(fd, "Invalid connection count");
        return -1;
    }

    topic_error = build_topic_manifest(g_topics_root,
                                       request.topic, &g_manifest);
    if (topic_error != TOPIC_OK) {
        (void)send_error_frame(fd, topic_error_to_string(topic_error));
        return -1;
    }

    if (create_work_plan(g_manifest, (int)requested, &new_plan) != 0) {
        (void)send_error_frame(fd, "Unable to create work plan");
        free_topic_manifest(g_manifest);
        g_manifest = NULL;
        return -1;
    }

    if (session_create(request.topic, g_manifest->manifest_fingerprint,
                       g_manifest->file_count, &g_session_id) != 0) {
        (void)send_error_frame(fd, "Unable to create session");
        destroy_work_plan(new_plan);
        free_topic_manifest(g_manifest);
        g_manifest = NULL;
        return -1;
    }

    g_plan = new_plan;
    g_plan->session_id = g_session_id;
    g_max_workers = (int)requested;

    memset(&accepted, 0, sizeof(accepted));
    accepted.session_id = enhanced_hton64(g_session_id);
    accepted.manifest_fingerprint =
        enhanced_hton32(g_manifest->manifest_fingerprint);
    accepted.file_count = enhanced_hton32(g_manifest->file_count);

    if (send_frame(fd, MSG_SESSION_ACCEPT, 0,
                   &accepted, sizeof(accepted)) != 0) {
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    char *endptr;
    long port_value;
    double failure_probability;
    int listener;
    int first_worker_started = 0;

    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <port> <topics_root_dir> <failure probability>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    errno = 0;
    port_value = strtol(argv[1], &endptr, 10);
    if (errno != 0 || *argv[1] == '\0' || *endptr != '\0' ||
        port_value < 1 || port_value > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return EXIT_FAILURE;
    }

    if (strlen(argv[2]) >= sizeof(g_topics_root)) {
        fprintf(stderr, "Topics root path is too long\n");
        return EXIT_FAILURE;
    }

    if (parse_failure_probability(argv[3], &failure_probability) != 0) {
        fprintf(stderr, "Failure probability must be between 0.0 and 1.0\n");
        return EXIT_FAILURE;
    }

    strcpy(g_topics_root, argv[2]);

    if (init_fault_injection(failure_probability, 0) != 0 ||
        session_table_init() != 0) {
        fprintf(stderr, "Unable to initialize server state\n");
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    listener = create_listener((uint16_t)port_value);
    if (listener < 0) {
        session_table_destroy();
        return EXIT_FAILURE;
    }

    printf("Enhanced Phase 5 server listening on port %ld\n", port_value);
    printf("Topics root: %s\n", g_topics_root);
    printf("Failure probability: %.3f\n", failure_probability);
    fflush(stdout);

    for (;;) {
        int fd = accept(listener, NULL, NULL);
        WorkerArgs *args;
        pthread_t thread;
        int worker_id;

        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&g_state_mutex);

        if (!first_worker_started) {
            if (initialize_session(fd) != 0) {
                pthread_mutex_unlock(&g_state_mutex);
                close(fd);
                continue;
            }

            first_worker_started = 1;
            worker_id = 0;
        } else {
            EnhancedWorkerHello hello;

            if (receive_payload(fd, MSG_WORK_ASSIGN,
                                &hello, sizeof(hello)) != 0) {
                pthread_mutex_unlock(&g_state_mutex);
                close(fd);
                continue;
            }

            hello.session_id = enhanced_ntoh64(hello.session_id);
            hello.worker_id = enhanced_ntoh32(hello.worker_id);
            worker_id = (int)hello.worker_id;

            if (hello.session_id != g_session_id ||
                worker_id < 0 || worker_id >= g_max_workers) {
                pthread_mutex_unlock(&g_state_mutex);
                (void)send_error_frame(fd, "Invalid worker session");
                close(fd);
                continue;
            }
        }

        pthread_mutex_unlock(&g_state_mutex);

        args = malloc(sizeof(*args));
        if (args == NULL) {
            close(fd);
            continue;
        }

        args->fd = fd;
        args->worker_id = worker_id;

        if (pthread_create(&thread, NULL, worker_main, args) != 0) {
            free(args);
            close(fd);
            continue;
        }

        pthread_detach(thread);
    }

    close(listener);
    return EXIT_SUCCESS;
}
