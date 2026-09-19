/*
 * SPSC ring buffer: one producer thread, one consumer thread, no locks.
 *
 * head and tail are free-running counters (they never wrap back to 0);
 * the slot is (index & mask), and (head - tail) is the number of items.
 * Each index has exactly one writer, so no shared counter is needed.
 *
 * Usage: ./rb [capacity]   (rounded up to a power of 2, default 1024)
 */

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct Ring {
    uint64_t capacity;
    uint64_t mask;
    uint64_t *buf;
    /* separate cache lines so producer and consumer don't false-share */
    alignas(64) _Atomic uint64_t head; /* next slot to write; stored only by producer */
    alignas(64) _Atomic uint64_t tail; /* next slot to read; stored only by consumer */
};

/* capacity must be a power of 2; returns 0 on success, -1 on failure */
int init_ring(struct Ring *ring, uint64_t capacity) {
    if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
        return -1;
    }
    ring->buf = malloc(capacity * sizeof(uint64_t));
    if (ring->buf == NULL) {
        return -1;
    }
    ring->capacity = capacity;
    ring->mask = capacity - 1;
    atomic_init(&ring->head, 0);
    atomic_init(&ring->tail, 0);
    return 0;
}

void kill_ring(struct Ring *ring) {
    free(ring->buf);
    ring->buf = NULL;
}

/* producer only; returns false if the ring is full */
bool ring_write(struct Ring *ring, uint64_t value) {
    uint64_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    /* acquire: the consumer must be done reading a slot before we reuse it */
    uint64_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    if (head - tail == ring->capacity) {
        return false;
    }
    ring->buf[head & ring->mask] = value;
    /* release: the slot's contents become visible before the new head does */
    atomic_store_explicit(&ring->head, head + 1, memory_order_release);
    return true;
}

/* consumer only; returns false if the ring is empty */
bool ring_read(struct Ring *ring, uint64_t *out) {
    uint64_t tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    uint64_t head = atomic_load_explicit(&ring->head, memory_order_acquire);
    if (head == tail) {
        return false;
    }
    *out = ring->buf[tail & ring->mask];
    atomic_store_explicit(&ring->tail, tail + 1, memory_order_release);
    return true;
}

/* smallest power of 2 >= c; returns 0 if that doesn't fit in 64 bits */
uint64_t next_pow2(uint64_t c) {
    if (c <= 1) {
        return 1;
    }
    c--;
    for (int shift = 1; shift < 64; shift <<= 1) {
        c |= c >> shift;
    }
    return c + 1;
}

static void check(bool ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", what);
        exit(1);
    }
}

static void test_single_thread(struct Ring *ring) {
    uint64_t v;
    check(!ring_read(ring, &v), "read from empty ring should fail");

    for (uint64_t i = 0; i < ring->capacity; i++) {
        check(ring_write(ring, i), "write into non-full ring");
    }
    check(!ring_write(ring, 999), "write into full ring should fail");

    for (uint64_t i = 0; i < ring->capacity; i++) {
        check(ring_read(ring, &v), "read from non-empty ring");
        check(v == i, "values come out in FIFO order");
    }
    check(!ring_read(ring, &v), "read from drained ring should fail");

    /* keep the ring half full while wrapping around several times */
    uint64_t next_in = 0, next_out = 0;
    for (uint64_t i = 0; i < ring->capacity / 2; i++) {
        check(ring_write(ring, next_in++), "prefill");
    }
    for (uint64_t i = 0; i < 5 * ring->capacity; i++) {
        check(ring_write(ring, next_in++), "write while wrapping");
        check(ring_read(ring, &v), "read while wrapping");
        check(v == next_out++, "FIFO order across wraparound");
    }
    while (ring_read(ring, &v)) {
        check(v == next_out++, "FIFO order while draining");
    }
    check(next_in == next_out, "every written value was read");

    puts("single-thread test: ok");
}

#define N_ITEMS (1UL << 22)

static void *producer(void *arg) {
    struct Ring *ring = arg;
    for (uint64_t i = 0; i < N_ITEMS; i++) {
        while (!ring_write(ring, i)) {
            /* spin until the consumer frees a slot */
        }
    }
    return NULL;
}

static void *consumer(void *arg) {
    struct Ring *ring = arg;
    uint64_t v;
    for (uint64_t i = 0; i < N_ITEMS; i++) {
        while (!ring_read(ring, &v)) {
            /* spin until the producer publishes a slot */
        }
        check(v == i, "two-thread FIFO order");
    }
    return NULL;
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void test_two_threads(struct Ring *ring) {
    pthread_t prod, cons;
    double start = now_seconds();
    if (pthread_create(&cons, NULL, consumer, ring) != 0 ||
        pthread_create(&prod, NULL, producer, ring) != 0) {
        fprintf(stderr, "pthread_create failed\n");
        exit(1);
    }
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    double elapsed = now_seconds() - start;

    printf("two-thread test:  ok\n");
    printf("items:            %lu\n", N_ITEMS);
    printf("elapsed:          %.6f s\n", elapsed);
    printf("items/sec:        %.2f\n", N_ITEMS / elapsed);
}

int main(int argc, char *argv[]) {
    uint64_t capacity = 1024;
    if (argc > 2) {
        fprintf(stderr, "usage: %s [capacity]\n", argv[0]);
        return 1;
    }
    if (argc == 2) {
        long c = atol(argv[1]);
        if (c <= 0) {
            fprintf(stderr, "capacity must be a positive int\n");
            return 1;
        }
        capacity = next_pow2((uint64_t)c);
    }

    struct Ring ring;
    if (init_ring(&ring, capacity) < 0) {
        fprintf(stderr, "init_ring failed for capacity %lu\n", capacity);
        return 1;
    }
    printf("capacity:         %lu\n", capacity);

    test_single_thread(&ring);
    test_two_threads(&ring);

    kill_ring(&ring);
    return 0;
}
