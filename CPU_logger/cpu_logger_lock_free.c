/*
 * Multi-CPU time/CPU-ID logger with a slow memcpy that doesn't block other CPUs.
 *
 * Idea: the only shared, serialized step is one atomic fetch_add that reserves
 * a slot. The slow memcpy then goes into that private slot with no lock held,
 * so every CPU copies in parallel. A per-slot sequence number (seqlock-style)
 * tells readers whether a slot is still being written or is committed.
 *
 * Build: gcc -O2 -std=c11 -pthread cpu_logger.c -o cpu_logger
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOG_SLOTS 4096     /* must be a power of two */
#define PAYLOAD_BYTES 4096 /* big record -> deliberately slow memcpy */
#define ENTRIES_PER_CPU 256

typedef struct {
    uint64_t timestamp_ns;
    uint32_t cpu_id;
    uint32_t len;
    uint8_t payload[PAYLOAD_BYTES];
} log_entry_t;

typedef struct {
    /* seq: 0 = never written, odd = write in progress, even = committed.
     * For ticket t: writing = 2t+1, committed = 2t+2. */
    _Alignas(64) _Atomic uint64_t seq;
    log_entry_t entry;
} log_slot_t;

typedef struct {
    _Alignas(64) _Atomic uint64_t head; /* next ticket to hand out */
    log_slot_t slots[LOG_SLOTS];
} log_buffer_t;

static log_buffer_t g_log;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Writer: safe to call concurrently from any number of CPUs. */
static void log_write(log_buffer_t *log, const log_entry_t *e) {
    /* 1. Reserve a slot: the ONLY contended operation, one atomic instruction. */
    uint64_t ticket = atomic_fetch_add_explicit(&log->head, 1, memory_order_relaxed);
    log_slot_t *s = &log->slots[ticket & (LOG_SLOTS - 1)];

    /* 2. Mark "in progress" before touching the data. */
    atomic_store_explicit(&s->seq, 2 * ticket + 1, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);

    /* 3. Slow copy, no lock held. Other CPUs are copying into their own slots. */
    memcpy(&s->entry, e, sizeof *e);

    /* 4. Publish: release makes the whole memcpy visible before the new seq. */
    atomic_store_explicit(&s->seq, 2 * ticket + 2, memory_order_release);
}

/* Reader: returns 0 if the entry for 'ticket' was read consistently. */
static int log_read(log_buffer_t *log, uint64_t ticket, log_entry_t *out) {
    log_slot_t *s = &log->slots[ticket & (LOG_SLOTS - 1)];
    const uint64_t committed = 2 * ticket + 2;

    if (atomic_load_explicit(&s->seq, memory_order_acquire) != committed)
        return -1; /* not written yet, or still in progress */

    memcpy(out, &s->entry, sizeof *out);

    atomic_thread_fence(memory_order_acquire);
    if (atomic_load_explicit(&s->seq, memory_order_relaxed) != committed)
        return -1; /* overwritten while we copied (wrap-around) */
    return 0;
}

static void *worker(void *arg) {
    int cpu = (int)(intptr_t)arg;

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof set, &set);

    static _Thread_local log_entry_t e; /* build the record locally first */
    for (int i = 0; i < ENTRIES_PER_CPU; i++) {
        e.timestamp_ns = now_ns();
        e.cpu_id = (uint32_t)sched_getcpu();
        e.len = PAYLOAD_BYTES;
        memset(e.payload, (int)(e.cpu_id & 0xFF), PAYLOAD_BYTES);
        log_write(&g_log, &e);
    }
    return NULL;
}

int main(void) {
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 1)
        ncpu = 1;
    if (ncpu * ENTRIES_PER_CPU > LOG_SLOTS) /* keep the demo wrap-free */
        ncpu = LOG_SLOTS / ENTRIES_PER_CPU;

    pthread_t *th = calloc((size_t)ncpu, sizeof *th);
    for (long c = 0; c < ncpu; c++)
        pthread_create(&th[c], NULL, worker, (void *)(intptr_t)c);
    for (long c = 0; c < ncpu; c++)
        pthread_join(th[c], NULL);

    /* Verify: every ticket committed, payload untorn. */
    uint64_t total = atomic_load(&g_log.head);
    uint64_t ok = 0, torn = 0, per_cpu[1024] = {0};
    static log_entry_t e;
    for (uint64_t t = 0; t < total; t++) {
        if (log_read(&g_log, t, &e) != 0)
            continue;
        int good = 1;
        for (int i = 0; i < PAYLOAD_BYTES; i++)
            if (e.payload[i] != (uint8_t)(e.cpu_id & 0xFF)) {
                good = 0;
                break;
            }
        if (!good) {
            torn++;
            continue;
        }
        ok++;
        if (e.cpu_id < 1024)
            per_cpu[e.cpu_id]++;
    }

    printf("CPUs: %ld, entries: %llu, valid: %llu, torn: %llu\n", ncpu, (unsigned long long)total,
           (unsigned long long)ok, (unsigned long long)torn);
    for (long c = 0; c < ncpu; c++)
        printf("  cpu %ld: %llu entries\n", c, (unsigned long long)per_cpu[c]);

    free(th);
    return (ok == total && torn == 0) ? 0 : 1;
}