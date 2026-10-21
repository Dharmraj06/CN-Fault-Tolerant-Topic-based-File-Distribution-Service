#!/usr/bin/env python3
"""Apply the measurement-instrumentation patch to common/protocol.c.

The patch is purely additive: it adds process-wide byte counters that are
updated inside send_all()/recv_all()/send_frame()/recv_frame_payload*(), and a
destructor that prints a single "[METRICS] ..." line to stderr ONLY when the
environment variable IT305_METRICS is set.  No protocol behaviour changes.
Line endings (CRLF) of the original file are preserved.
"""
import sys

path = sys.argv[1]
raw = open(path, "rb").read().decode("utf-8")
crlf = "\r\n" in raw
text = raw.replace("\r\n", "\n")

if "IT305_METRICS" in text:
    print("already patched")
    sys.exit(0)


def sub(old, new, count=1):
    global text
    assert text.count(old) == count, (old, text.count(old))
    text = text.replace(old, new)


sub('#include <arpa/inet.h>\n\n#if defined(__linux__)',
    '''#include <arpa/inet.h>

/* ----------------------------------------------------------------------------
 * Measurement instrumentation (added for the benchmark campaign).
 * Process-wide application-level byte counters.  They never alter protocol
 * behaviour and are only reported when the IT305_METRICS environment variable
 * is set (one "[METRICS]" line on stderr at process exit).
 *   wire_bytes_*   : every byte handed to send() / returned by recv()
 *                    (frame headers + payloads, i.e. application-level framed
 *                    bytes, excluding TCP/IP headers).
 *   data_payload_* : original file-content bytes carried inside MSG_FILE_DATA
 *                    and MSG_WORK_DATA frames (the 20-byte work-data sub-header
 *                    of MSG_WORK_DATA is excluded).
 * ---------------------------------------------------------------------------- */
#define METRIC_WORK_DATA_SUBHEADER 20u /* sizeof(EnhancedWorkDataHeader) */
static uint64_t g_wire_bytes_sent = 0;
static uint64_t g_wire_bytes_recv = 0;
static uint64_t g_data_payload_sent = 0;
static uint64_t g_data_payload_recv = 0;

static void metric_add(uint64_t *counter, uint64_t value) {
    __atomic_fetch_add(counter, value, __ATOMIC_RELAXED);
}

static uint64_t metric_data_bytes(uint8_t type, uint64_t payload_len) {
    if (type == MSG_FILE_DATA) {
        return payload_len;
    }
    if (type == MSG_WORK_DATA && payload_len >= METRIC_WORK_DATA_SUBHEADER) {
        return payload_len - METRIC_WORK_DATA_SUBHEADER;
    }
    return 0;
}

__attribute__((destructor)) static void proto_metrics_report(void) {
    if (getenv("IT305_METRICS") == NULL) {
        return;
    }
    fprintf(stderr,
            "[METRICS] wire_bytes_sent=%llu wire_bytes_recv=%llu "
            "data_payload_sent=%llu data_payload_recv=%llu\\n",
            (unsigned long long)__atomic_load_n(&g_wire_bytes_sent, __ATOMIC_RELAXED),
            (unsigned long long)__atomic_load_n(&g_wire_bytes_recv, __ATOMIC_RELAXED),
            (unsigned long long)__atomic_load_n(&g_data_payload_sent, __ATOMIC_RELAXED),
            (unsigned long long)__atomic_load_n(&g_data_payload_recv, __ATOMIC_RELAXED));
}

#if defined(__linux__)''')

sub('        total_sent += (size_t)n;\n',
    '        total_sent += (size_t)n;\n        metric_add(&g_wire_bytes_sent, (uint64_t)n);\n')
sub('        total_received += (size_t)n;\n',
    '        total_received += (size_t)n;\n        metric_add(&g_wire_bytes_recv, (uint64_t)n);\n')

# send_frame: count data payload after the payload was fully handed to send()
sub('''        if (send_all(fd, payload, (size_t)payload_len) != 0) {
            return -1;
        }
    }

    return 0;
}''',
    '''        if (send_all(fd, payload, (size_t)payload_len) != 0) {
            return -1;
        }
        metric_add(&g_data_payload_sent, metric_data_bytes(type, payload_len));
    }

    return 0;
}''')

# recv_frame_payload: count after a successful full payload read
sub('''    int rc = recv_all(fd, buf, (size_t)hdr->payload_len);
    if (rc != 0) {
        /* Premature EOF mid-payload or socket error is a fatal error */
        return -1;
    }

    return 0;''',
    '''    int rc = recv_all(fd, buf, (size_t)hdr->payload_len);
    if (rc != 0) {
        /* Premature EOF mid-payload or socket error is a fatal error */
        return -1;
    }
    metric_add(&g_data_payload_recv, metric_data_bytes(hdr->type, hdr->payload_len));

    return 0;''')

sub('''    *out_buf = mem;
    return 0;''',
    '''    metric_add(&g_data_payload_recv, metric_data_bytes(hdr->type, hdr->payload_len));
    *out_buf = mem;
    return 0;''')

if crlf:
    text = text.replace("\n", "\r\n")
open(path, "wb").write(text.encode("utf-8"))
print("patched", path, "(CRLF preserved)" if crlf else "")
