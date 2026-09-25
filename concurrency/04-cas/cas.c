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

struct worker_arg {
    long int iter;
    atomic_llong *cnt;
};

void *worker_explicit(void *args) {
    struct worker_arg *arg = args;
    for (long int i = 0; i < arg->iter; i++) {
        atomic_fetch_add_explicit(arg->cnt, 1, memory_order_relaxed);
    }
    return NULL;
}

void *worker_generic(void *args) {
    struct worker_arg *arg = args;
    for (long int i = 0; i < arg->iter; i++) {
        atomic_fetch_add(arg->cnt, 1);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage mutex [num_threads] [reps] [generic|explicit]\n");
        return 1;
    }
    void *(*fn)(void *);
    if (strcmp(argv[3], "generic") == 0) {
        fn = worker_generic;
    } else if (strcmp(argv[3], "explicit") == 0) {
        fn = worker_explicit;
    } else {
        fprintf(stderr, "lock must be generic or explicit\n");
        return 1;
    }
    int num_thread = atoi(argv[1]);
    if (num_thread <= 0) {
        fprintf(stderr, "cannot convert the number of threads\n");
        return 1;
    }
    pthread_t *thds = malloc(num_thread * sizeof(pthread_t));
    if (thds == NULL) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    struct worker_arg args;
    if (!(args.iter = atol(argv[2]))) {
        fprintf(stderr, "cannot convert the number of reps\n");
        free(thds);
        return 1;
    }
    atomic_llong counter = 0;
    args.cnt = &counter;

    for (int i = 0; i < num_thread; i++) {
        if (pthread_create(&thds[i], NULL, fn, &args) != 0) {
            fprintf(stderr, "thread %d could not be created\n", i);
            free(thds);
            return 1;
        }
    }

    for (int i = 0; i < num_thread; i++) {
        if (pthread_join(thds[i], NULL) != 0) {
            fprintf(stderr, "thread %d did not finish\n", i);
            free(thds);
            return 1;
        }
    }
    printf("counter with %s is %lld\n", argv[3], *(args.cnt));
    assert(*(args.cnt) == (long long int)args.iter * num_thread);
    free(thds);
    return 0;
}
