#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct worker_arg {
    long int iter;
    long long int *counter;
    pthread_mutex_t *lock;
};

void *worker(void *args) {
    struct worker_arg *arg = args;
    for (long int i = 0; i < arg->iter; i++) {
        pthread_mutex_lock(arg->lock);
        (*(arg->counter))++;
        pthread_mutex_unlock(arg->lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage mutex [num_threads] [reps]\n");
        return 1;
    }
    int num_thread = atoi(argv[1]);
    pthread_t *thds = (pthread_t *)malloc(num_thread * sizeof(pthread_t));
    if (thds == NULL) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    struct worker_arg args;
    long long int c = 0;
    if (!(args.iter = atol(argv[2]))) {
        fprintf(stderr, "cannot convert the number of reps\n");
        free(thds);
        return 1;
    }
    args.counter = &c;
    pthread_mutex_t lock;
    args.lock = &lock;
    if (pthread_mutex_init(args.lock, NULL) != 0) {
        fprintf(stderr, "could not init lock\n");
        free(thds);
        return 1;
    }

    for (int i = 0; i < num_thread; i++) {
        if (pthread_create(&thds[i], NULL, &worker, &args) != 0) {
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
    printf("counter is %lld\n", *(args.counter));
    free(thds);
    return 0;
}
