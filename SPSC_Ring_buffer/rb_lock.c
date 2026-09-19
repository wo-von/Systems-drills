/*
 * SPSC ring buffer, lock-based version: a mutex guards head/tail, and two
 * condition variables let a thread sleep instead of spinning when the ring
 * is full (producer) or empty (consumer).
 *
 * Same free-running head/tail scheme as rb.c; compare throughput with it.
 *
 * Usage: ./rb_lock [capacity]   (rounded up to a power of 2, default 1024)
 */

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct Ring {
    uint64_t capacity;
    uint64_t mask;
    uint64_t *buf;
    uint64_t head; /* guarded by lock */
    uint64_t tail; /* guarded by lock */
    pthread_mutex_t lock;
    pthread_cond_t not_full;  /* producer waits on this */
    pthread_cond_t not_empty; /* consumer waits on this */
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
    ring->head = 0;
    ring->tail = 0;
    pthread_mutex_init(&ring->lock, NULL);
    pthread_cond_init(&ring->not_full, NULL);
    pthread_cond_init(&ring->not_empty, NULL);
    return 0;
}

void kill_ring(struct Ring *ring) {
    pthread_cond_destroy(&ring->not_empty);
    pthread_cond_destroy(&ring->not_full);
    pthread_mutex_destroy(&ring->lock);
    free(ring->buf);
    ring->buf = NULL;
}

/* non-blocking; returns false if the ring is full */
bool ring_try_write(struct Ring *ring, uint64_t value) {
    pthread_mutex_lock(&ring->lock);
    if (ring->head - ring->tail == ring->capacity) {
        pthread_mutex_unlock(&ring->lock);
        return false;
    }
    ring->buf[ring->head & ring->mask] = value;
    ring->head++;
    pthread_cond_signal(&ring->not_empty);
    pthread_mutex_unlock(&ring->lock);
    return true;
}

/* non-blocking; returns false if the ring is empty */
bool ring_try_read(struct Ring *ring, uint64_t *out) {
    pthread_mutex_lock(&ring->lock);
    if (ring->head == ring->tail) {
        pthread_mutex_unlock(&ring->lock);
        return false;
    }
    *out = ring->buf[ring->tail & ring->mask];
    ring->tail++;
    pthread_cond_signal(&ring->not_full);
    pthread_mutex_unlock(&ring->lock);
    return true;
}

/* blocks while the ring is full */
void ring_write(struct Ring *ring, uint64_t value) {
    pthread_mutex_lock(&ring->lock);
    /* while, not if: wakeups can be spurious, so re-check after waking */
    while (ring->head - ring->tail == ring->capacity) {
        pthread_cond_wait(&ring->not_full, &ring->lock);
    }
    ring->buf[ring->head & ring->mask] = value;
    ring->head++;
    pthread_cond_signal(&ring->not_empty);
    pthread_mutex_unlock(&ring->lock);
}

/* blocks while the ring is empty */
uint64_t ring_read(struct Ring *ring) {
    pthread_mutex_lock(&ring->lock);
    while (ring->head == ring->tail) {
        pthread_cond_wait(&ring->not_empty, &ring->lock);
    }
    uint64_t value = ring->buf[ring->tail & ring->mask];
    ring->tail++;
    pthread_cond_signal(&ring->not_full);
    pthread_mutex_unlock(&ring->lock);
    return value;
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
    check(!ring_try_read(ring, &v), "read from empty ring should fail");

    for (uint64_t i = 0; i < ring->capacity; i++) {
        check(ring_try_write(ring, i), "write into non-full ring");
    }
    check(!ring_try_write(ring, 999), "write into full ring should fail");

    for (uint64_t i = 0; i < ring->capacity; i++) {
        check(ring_try_read(ring, &v), "read from non-empty ring");
        check(v == i, "values come out in FIFO order");
    }
    check(!ring_try_read(ring, &v), "read from drained ring should fail");

    /* keep the ring half full while wrapping around several times */
    uint64_t next_in = 0, next_out = 0;
    for (uint64_t i = 0; i < ring->capacity / 2; i++) {
        check(ring_try_write(ring, next_in++), "prefill");
    }
    for (uint64_t i = 0; i < 5 * ring->capacity; i++) {
        check(ring_try_write(ring, next_in++), "write while wrapping");
        check(ring_try_read(ring, &v), "read while wrapping");
        check(v == next_out++, "FIFO order across wraparound");
    }
    while (ring_try_read(ring, &v)) {
        check(v == next_out++, "FIFO order while draining");
    }
    check(next_in == next_out, "every written value was read");

    puts("single-thread test: ok");
}

#define N_ITEMS (1UL << 22)

static void *producer(void *arg) {
    struct Ring *ring = arg;
    for (uint64_t i = 0; i < N_ITEMS; i++) {
        ring_write(ring, i);
    }
    return NULL;
}

static void *consumer(void *arg) {
    struct Ring *ring = arg;
    for (uint64_t i = 0; i < N_ITEMS; i++) {
        check(ring_read(ring) == i, "two-thread FIFO order");
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
