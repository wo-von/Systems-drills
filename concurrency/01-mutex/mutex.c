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
    long long int *counter;
};

pthread_mutex_t c_lock;
atomic_int lock_word; // must be 4 bytes

// 0 = free, 1 = locked and nobody waiting, 2 = locked and maybe someone waiting
void my_lock(atomic_int *word) {
    // fast path: free -> 1, no syscall
    if (atomic_exchange(word, 1) == 0)
        return;
    // slow path: mark "maybe waiters" before sleeping, so the unlocker knows
    // to wake us. A thread that gets the lock here leaves 2 in the word,
    // because it can't know whether others are still asleep.
    while (atomic_exchange(word, 2) != 0) {
        // sleep while the word is still 2. EAGAIN (it already changed) or
        // EINTR just mean try again, so the result is ignored.
        syscall(SYS_futex, word, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
    }
}

void my_unlock(atomic_int *word) {
    // release and read the old value in one step; only 2 can mean sleepers
    if (atomic_exchange(word, 0) == 2)
        syscall(SYS_futex, word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

void *worker(void *args) {
    struct worker_arg *arg = args;
    for (long int i = 0; i < arg->iter; i++) {
        my_lock(&lock_word);
        (*(arg->counter))++;
        my_unlock(&lock_word);
    }
    return NULL;
}

void *worker_lock(void *args) {
    struct worker_arg *arg = args;
    for (long int i = 0; i < arg->iter; i++) {
        pthread_mutex_lock(&c_lock);
        (*(arg->counter))++;
        pthread_mutex_unlock(&c_lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage mutex [num_threads] [reps] [pthread|mine]\n");
        return 1;
    }
    void *(*fn)(void *);
    if (strcmp(argv[3], "pthread") == 0) {
        fn = worker_lock;
    } else if (strcmp(argv[3], "mine") == 0) {
        fn = worker;
    } else {
        fprintf(stderr, "lock must be pthread or mine\n");
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
    long long int c = 0;
    args.counter = &c;
    if (pthread_mutex_init(&c_lock, NULL) != 0) {
        fprintf(stderr, "could not init lock\n");
        free(thds);
        return 1;
    }

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
    printf("counter with %s is %lld\n", argv[3], *(args.counter));
    assert(*(args.counter) == (long long int)args.iter * num_thread);
    pthread_mutex_destroy(&c_lock);
    free(thds);
    return 0;
}
