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

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

struct worker_arg {
    long int iter;
    atomic_long *cnt;
    long retries; // written by its own thread only, read by main after join
    int id;       // 0 .. num_thread-1, for the values my_max offers
    int num_thread;
};

long my_add(atomic_long *p, long v, long *retries) {
    long p_snapshot = atomic_load_explicit(p, memory_order_acquire);
    long add = p_snapshot + v;
    while (!atomic_compare_exchange_weak(p, &p_snapshot, add)) {
        add = p_snapshot + v;
        (*retries)++;
    }
    return p_snapshot;
}

long my_max(atomic_long *p, long v, long *retries) {
    long p_snapshot = atomic_load_explicit(p, memory_order_acquire);
    // store only while v is still bigger than what is there; a failed CAS
    // refreshes p_snapshot, so the condition re-checks against the new value
    while (v > p_snapshot) {
        if (atomic_compare_exchange_weak(p, &p_snapshot, v)) {
            break;
        }
        (*retries)++;
    }
    return p_snapshot;
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

// each thread offers i * num_thread + id: every value 0 .. iter*num_thread-1
// exactly once, spread over all threads
void *worker_my_max(void *args) {
    struct worker_arg *arg = args;
    long retries = 0;
    for (long int i = 0; i < arg->iter; i++) {
        my_max(arg->cnt, i * arg->num_thread + arg->id, &retries);
    }
    arg->retries = retries;
    return NULL;
}

// start num_thread threads running fn, wait for all, return the summed retries
// (-1 if a thread could not be created or joined)
long run_threads(pthread_t *thds, struct worker_arg *args, int num_thread, void *(*fn)(void *)) {
    for (int i = 0; i < num_thread; i++) {
        args[i].retries = 0;
        if (pthread_create(&thds[i], NULL, fn, &args[i]) != 0) {
            fprintf(stderr, "thread %d could not be created\n", i);
            return -1;
        }
    }
    long total_retries = 0;
    for (int i = 0; i < num_thread; i++) {
        if (pthread_join(thds[i], NULL) != 0) {
            fprintf(stderr, "thread %d did not finish\n", i);
            return -1;
        }
        total_retries += args[i].retries;
    }
    return total_retries;
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
        args[i].id = i;
        args[i].num_thread = num_thread;
    }
    // first the library fetch_add
    if (run_threads(thds, args, num_thread, worker_generinc) < 0) {
        free(thds);
        free(args);
        return 1;
    }
    printf("counter with generic is %ld\n", counter);
    assert(counter == iter * num_thread);

    // now my_add
    counter = 0;
    long retries = run_threads(thds, args, num_thread, worker_my_add);
    if (retries < 0) {
        free(thds);
        free(args);
        return 1;
    }
    printf("counter with my_add is %ld, retries %ld\n", counter, retries);
    assert(counter == iter * num_thread);

    // now my_max: the largest value offered is iter * num_thread - 1
    counter = 0;
    retries = run_threads(thds, args, num_thread, worker_my_max);
    if (retries < 0) {
        free(thds);
        free(args);
        return 1;
    }
    printf("max with my_max is %ld, retries %ld\n", counter, retries);
    assert(counter == iter * num_thread - 1);

    free(thds);
    free(args);
    return 0;
}
