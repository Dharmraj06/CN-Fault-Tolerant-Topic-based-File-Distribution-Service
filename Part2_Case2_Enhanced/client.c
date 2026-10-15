
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
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
#include "enhanced_protocol.h"

typedef struct {
    char path[MAX_PATH_BUFFER_LEN];
    uint64_t size;
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
    char buffer[MAX_PATH_BUFFER_LEN * 2];
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
        char full_path[MAX_PATH_BUFFER_LEN * 2];
        char parent[MAX_PATH_BUFFER_LEN * 2];
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
                    enhanced_ntoh64(metadata.file_size)) {
                return -1;
            }
            continue;
        }

        snprintf(session->files[i].path,
                 sizeof(session->files[i].path), "%s",
                 metadata.relative_path);

        session->files[i].size =
            enhanced_ntoh64(metadata.file_size);

        if (snprintf(full_path, sizeof(full_path), "%s/%s",
                     session->output_dir,
                     session->files[i].path) >= (int)sizeof(full_path)) {
            return -1;
        }

        snprintf(parent, sizeof(parent), "%s", full_path);
        slash = strrchr(parent, '/');

        if (slash != NULL) {
            *slash = '\0';

            if (make_directories(parent) != 0) {
                perror("Creating parent directory");
                return -1;
            }
        }

        output_fd = open(full_path, O_CREAT | O_RDWR, 0666);

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

    fprintf(stderr, "[worker %d] Manifest received: %u files, %u units\n",
            worker_id, session->file_count, session->unit_count);

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

            fprintf(stderr, "[worker %d] Transfer complete\n", worker_id);
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

            fprintf(stderr, "[worker %d] Assigned unit %u\n",
                    worker_id, enhanced_ntoh32(assignment.unit_id));
            continue;
        }

        if (header.type == MSG_WORK_DATA) {
            EnhancedWorkDataHeader data_header;
            unsigned char *payload;
            uint32_t unit_id;
            uint32_t file_index;
            uint32_t data_length;
            uint64_t file_offset;
            char output_path[MAX_PATH_BUFFER_LEN * 2];
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

            if (snprintf(output_path, sizeof(output_path), "%s/%s",
                         session->output_dir,
                         session->files[file_index].path) >=
                    (int)sizeof(output_path)) {
                free(payload);
                return -1;
            }

            output_fd = open(output_path, O_RDWR);

            if (output_fd < 0) {
                perror("Opening output file");
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
                sleep(1);
                continue;
            }

            if (send_worker_hello(fd, session, worker_id) != 0) {
                close(fd);
                fd = -1;
                sleep(1);
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
        sleep(1);
    }

    return NULL;
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
    ClientSession session;
    MsgSessionStartPayload request;
    MsgSessionAcceptPayload accepted;
    pthread_t threads[ENHANCED_MAX_CONNECTIONS];
    int thread_count = 0;
    int first_fd;
    uint16_t port;
    long connections = 1;
    char *end;
    int i;

    if (argc != 5 && argc != 6) {
        fprintf(stderr,
                "Usage: %s <server_ip> <server_port> <topic> "
                "<output_dir> [num_connections]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_port(argv[2], &port) != 0) {
        fprintf(stderr, "Invalid server port\n");
        return EXIT_FAILURE;
    }

    if (argc == 6) {
        errno = 0;
        connections = strtol(argv[5], &end, 10);

        if (errno != 0 || argv[5][0] == '\0' || *end != '\0' ||
            connections < 1 ||
            connections > ENHANCED_MAX_CONNECTIONS) {
            fprintf(stderr, "Invalid number of connections (1-%d)\n",
                    ENHANCED_MAX_CONNECTIONS);
            return EXIT_FAILURE;
        }
    }

    memset(&session, 0, sizeof(session));

    if (strlen(argv[1]) >= sizeof(session.ip) ||
        strlen(argv[4]) >= sizeof(session.output_dir) ||
        strlen(argv[3]) >= sizeof(request.topic)) {
        fprintf(stderr, "An argument is too long\n");
        return EXIT_FAILURE;
    }

    strcpy(session.ip, argv[1]);
    strcpy(session.output_dir, argv[4]);
    session.port = port;

    if (pthread_mutex_init(&session.manifest_mutex, NULL) != 0) {
        fprintf(stderr, "Could not initialize manifest mutex\n");
        return EXIT_FAILURE;
    }

    if (make_directories(session.output_dir) != 0) {
        perror("Creating output directory");
        pthread_mutex_destroy(&session.manifest_mutex);
        return EXIT_FAILURE;
    }

    first_fd = connect_server(&session);

    if (first_fd < 0) {
        perror("Connecting to server");
        pthread_mutex_destroy(&session.manifest_mutex);
        return EXIT_FAILURE;
    }

    memset(&request, 0, sizeof(request));
    strcpy(request.topic, argv[3]);
    request.requested_conns = htonl((uint32_t)connections);

    if (send_frame(first_fd, MSG_SESSION_START, 0,
                   &request, sizeof(request)) != 0 ||
        receive_expected(first_fd, MSG_SESSION_ACCEPT,
                         &accepted, sizeof(accepted)) != 0) {
        fprintf(stderr, "Session initialization failed\n");
        close(first_fd);
        pthread_mutex_destroy(&session.manifest_mutex);
        return EXIT_FAILURE;
    }

    session.session_id = enhanced_ntoh64(accepted.session_id);
    session.fingerprint =
        enhanced_ntoh32(accepted.manifest_fingerprint);

    fprintf(stderr,
            "Session accepted: ID=%llu, files=%u, connections=%ld\n",
            (unsigned long long)session.session_id,
            enhanced_ntoh32(accepted.file_count), connections);

    for (i = 0; i < (int)connections; ++i) {
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

    free(session.files);
    pthread_mutex_destroy(&session.manifest_mutex);

    if (thread_count != (int)connections) {
        return EXIT_FAILURE;
    }

    printf("Client finished.\n");
    return EXIT_SUCCESS;
}
