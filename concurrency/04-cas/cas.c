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
    atomic_long *cnt;
    long retries; // written by its own thread only, read by main after join
};

long my_add(atomic_long *p, long v, long *retries) {
    long atom_p = atomic_load_explicit(p, memory_order_acquire);
    long add = atom_p + v;
    while (!atomic_compare_exchange_weak(p, &atom_p, add)) {
        add = atom_p + v;
        (*retries)++;
    }
    return atom_p;
}

void *worker_generinc(void *args) {
    struct worker_arg *arg = args;
    for (long int i = 0; i < arg->iter; i++) {
        atomic_fetch_add_explicit(arg->cnt, 1, memory_order_relaxed);
    }
    return NULL;
}

void *worker_my_add(void *args) {
    struct worker_arg *arg = args;
    long retries = 0;
    for (long int i = 0; i < arg->iter; i++) {
        my_add(arg->cnt, 1, &retries);
    }
    arg->retries = retries;
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: cas [num_threads] [reps]\n");
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

    long iter = atol(argv[2]);
    if (iter <= 0) {
        fprintf(stderr, "cannot convert the number of reps\n");
        free(thds);
        return 1;
    }
    // one struct per thread, so each has its own retries slot
    struct worker_arg *args = malloc(num_thread * sizeof(struct worker_arg));
    if (args == NULL) {
        fprintf(stderr, "malloc failed\n");
        free(thds);
        return 1;
    }
    atomic_long counter = 0;
    for (int i = 0; i < num_thread; i++) {
        args[i].iter = iter;
        args[i].cnt = &counter;
        args[i].retries = 0;
    }
    // first try generic_add
    for (int i = 0; i < num_thread; i++) {
        if (pthread_create(&thds[i], NULL, worker_generinc, &args[i]) != 0) {
            fprintf(stderr, "thread %d could not be created\n", i);
            free(thds);
            free(args);
            return 1;
        }
    }

    for (int i = 0; i < num_thread; i++) {
        if (pthread_join(thds[i], NULL) != 0) {
            fprintf(stderr, "thread %d did not finish\n", i);
            free(thds);
            free(args);
            return 1;
        }
    }
    printf("counter with generic is %ld\n", counter);
    assert(counter == iter * num_thread);

    // now try my_add
    counter = 0;
    for (int i = 0; i < num_thread; i++) {
        if (pthread_create(&thds[i], NULL, worker_my_add, &args[i]) != 0) {
            fprintf(stderr, "thread %d could not be created\n", i);
            free(thds);
            free(args);
            return 1;
        }
    }

    for (int i = 0; i < num_thread; i++) {
        if (pthread_join(thds[i], NULL) != 0) {
            fprintf(stderr, "thread %d did not finish\n", i);
            free(thds);
            free(args);
            return 1;
        }
    }
    long total_retries = 0;
    for (int i = 0; i < num_thread; i++) {
        total_retries += args[i].retries;
    }
    printf("counter with my_add is %ld, retries %ld\n", counter, total_retries);
    assert(counter == iter * num_thread);

    free(thds);
    free(args);
    return 0;
}
