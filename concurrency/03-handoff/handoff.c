#define _GNU_SOURCE
#include <assert.h>
#include <linux/futex.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

struct Log {
    int data[4];
    uint64_t time;
    int cpuid;
};

uint64_t time_saved;

atomic_int flag; // 0 empty, 1 full

uint64_t time_now(void) {
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret != 0) {
        perror("time_now failed");
        return 0;
    }
    // nanoseconds; tv_sec + tv_nsec / 1e9 is a double and the cast drops the fraction
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

// read the published struct and check every field; call only after an
// acquire load has seen flag == 1
void check_log(struct Log *log) {
    int id = log->cpuid;
    uint64_t then = log->time;
    int copy[4];
    for (int i = 0; i < 4; i++) {
        copy[i] = log->data[i];
    }
    assert(id == 12);
    for (int i = 0; i < 4; i++) {
        assert(copy[i] == i);
    }
    assert(then == time_saved);
}

// check the flag once: safe, but may do nothing (no progress guarantee)
void *read_try(void *args) {
    if (atomic_load_explicit(&flag, memory_order_acquire)) {
        check_log(args);
        return NULL;
    } else {
        return (void *)1;
    }
}

// spin until the flag is set: always reads it eventually, burns a core meanwhile
void *read_spin(void *args) {
    while (!atomic_load_explicit(&flag, memory_order_acquire)) {
        ;
    }
    check_log(args);
    return NULL;
}

void *write_handoff(void *args) {

    struct Log *log = args;
    log->time = time_now();
    log->cpuid = 12;
    log->data[0] = 0;
    log->data[1] = 1;
    log->data[2] = 2;
    log->data[3] = 3;
    time_saved = log->time;
    atomic_store_explicit(&flag, 1, memory_order_release);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: handoff spin|try\n");
        return 1;
    }
    void *(*read_handoff)(void *);
    if (strcmp(argv[1], "spin") == 0) {
        read_handoff = read_spin;
    } else if (strcmp(argv[1], "try") == 0) {
        read_handoff = read_try;
    } else {
        fprintf(stderr, "mode must be spin or try\n");
        return 1;
    }

    pthread_t reader, writer;
    struct Log our_log;
    atomic_store(&flag, 0);
    int ret = pthread_create(&writer, NULL, write_handoff, &our_log);
    if (ret) {
        fprintf(stderr, "Cannot create thread\n");
        return 1;
    }

    ret = pthread_create(&reader, NULL, read_handoff, &our_log);
    if (ret) {
        fprintf(stderr, "Cannot create thread\n");
        return 1;
    }
    void *res;
    if (pthread_join(writer, NULL) != 0 || pthread_join(reader, &res) != 0) {
        fprintf(stderr, "join failed\n");
        return 1;
    }
    if (res == NULL) {
        printf("ready\n");
    } else {
        printf("not ready\n");
    }
    return 0;
}
