/*
 * Lock-based multi-CPU logger with a busy flag and condvar signaling.
 *
 * - One CPU at a time owns the log ("busy"). The mutex only guards the flag
 *   for a few instructions; it is NOT held during the slow memcpy.
 * - A CPU that finds the log busy doesn't wait: it keeps preparing its own
 *   messages into a local queue (batching).
 * - Only when its local queue is full does it sleep on a condition variable,
 *   and the current owner signals (broadcasts) when it is done.
 *
 * Build: gcc -O2 -std=c11 -pthread cpu_logger_lock.c -o cpu_logger_lock
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOG_CAPACITY 4096
#define PAYLOAD_BYTES 4096 /* big record -> slow memcpy */
#define ENTRIES_PER_CPU 256
#define LOCAL_CAP 8 /* messages a CPU may stage while the log is busy */

typedef struct {
    uint64_t timestamp_ns;
    uint32_t cpu_id;
    uint32_t len;
    uint8_t payload[PAYLOAD_BYTES];
} log_entry_t;

/* ---- shared log ---------------------------------------------------------- */
static log_entry_t g_log[LOG_CAPACITY];
static size_t g_count; /* touched only by the busy owner */

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_free = PTHREAD_COND_INITIALIZER;
static int g_busy; /* guarded by g_mtx */

/* "I'm busy" if nobody else is. Never blocks. */
static int try_begin_write(void) {
    int got = 0;
    pthread_mutex_lock(&g_mtx);
    if (!g_busy) {
        g_busy = 1;
        got = 1;
    }
    pthread_mutex_unlock(&g_mtx);
    return got;
}

/* Sleep until the log is free, then claim it. */
static void wait_begin_write(void) {
    pthread_mutex_lock(&g_mtx);
    while (g_busy) /* loop: spurious wakeups */
        pthread_cond_wait(&g_free, &g_mtx);
    g_busy = 1;
    pthread_mutex_unlock(&g_mtx);
}

/* "I'm done": clear the flag and signal anyone sleeping. */
static void end_write(void) {
    pthread_mutex_lock(&g_mtx);
    g_busy = 0;
    pthread_cond_broadcast(&g_free);
    pthread_mutex_unlock(&g_mtx);
}

/* Slow part: runs with no mutex held, only the busy flag owned. */
static void flush(const log_entry_t *pending, int n) {
    for (int i = 0; i < n && g_count < LOG_CAPACITY; i++)
        memcpy(&g_log[g_count++], &pending[i], sizeof pending[i]);
}

/* ---- per-CPU worker ------------------------------------------------------ */
typedef struct {
    int cpu;
    unsigned overlapped, slept;
} worker_arg_t;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void make_entry(log_entry_t *e, int cpu) {
    e->timestamp_ns = now_ns();
    e->cpu_id = (uint32_t)cpu; /* or sched_getcpu() when pinned */
    e->len = PAYLOAD_BYTES;
    memset(e->payload, cpu & 0xFF, PAYLOAD_BYTES);
}

static void *worker(void *p) {
    worker_arg_t *a = p;

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(a->cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof set, &set);

    static _Thread_local log_entry_t pending[LOCAL_CAP];
    int n = 0;

    for (int i = 0; i < ENTRIES_PER_CPU; i++) {
        make_entry(&pending[n++], a->cpu); /* our own work */

        if (try_begin_write()) { /* log free: flush everything staged */
            flush(pending, n);
            n = 0;
            end_write();
        } else if (n == LOCAL_CAP) { /* busy AND nowhere left to stage */
            a->slept++;
            wait_begin_write();
            flush(pending, n);
            n = 0;
            end_write();
        } else {
            a->overlapped++; /* busy: keep working instead of waiting */
        }
    }

    if (n) { /* drain leftovers */
        wait_begin_write();
        flush(pending, n);
        end_write();
    }
    return NULL;
}

int main(int argc, char **argv) {
    long ncpu = (argc > 1) ? atol(argv[1]) : sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 1)
        ncpu = 1;
    if (ncpu * ENTRIES_PER_CPU > LOG_CAPACITY)
        ncpu = LOG_CAPACITY / ENTRIES_PER_CPU;

    pthread_t *th = calloc((size_t)ncpu, sizeof *th);
    worker_arg_t *args = calloc((size_t)ncpu, sizeof *args);
    for (long c = 0; c < ncpu; c++) {
        args[c].cpu = (int)c;
        pthread_create(&th[c], NULL, worker, &args[c]);
    }
    for (long c = 0; c < ncpu; c++)
        pthread_join(th[c], NULL);

    /* Verify: right count, no torn payloads, per-CPU timestamps in order. */
    size_t torn = 0, unordered = 0;
    uint64_t last_ts[256] = {0};
    for (size_t i = 0; i < g_count; i++) {
        const log_entry_t *e = &g_log[i];
        for (int b = 0; b < PAYLOAD_BYTES; b++)
            if (e->payload[b] != (uint8_t)(e->cpu_id & 0xFF)) {
                torn++;
                break;
            }
        if (e->cpu_id < 256) {
            if (e->timestamp_ns < last_ts[e->cpu_id])
                unordered++;
            last_ts[e->cpu_id] = e->timestamp_ns;
        }
    }

    printf("CPUs: %ld, entries: %zu (expected %ld), torn: %zu, out-of-order: %zu\n", ncpu, g_count,
           ncpu * ENTRIES_PER_CPU, torn, unordered);
    for (long c = 0; c < ncpu; c++)
        printf("  cpu %ld: worked while busy %u times, slept %u times\n", c, args[c].overlapped,
               args[c].slept);

    int ok = (g_count == (size_t)(ncpu * ENTRIES_PER_CPU)) && !torn && !unordered;
    free(th);
    free(args);
    return ok ? 0 : 1;
}