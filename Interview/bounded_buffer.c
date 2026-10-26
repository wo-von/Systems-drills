#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct bq {
    int *buf;
    size_t head;  // to write to
    size_t tail;  // to read from
    size_t count; // how many have been written
    size_t cap;   // max capacity
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full;
};
// Undoes a successful bq_init. Only call it on a queue whose bq_init
// returned true, and only once no thread is still using the queue.
void bq_destroy(struct bq *q) {
    pthread_cond_destroy(&q->cond_not_full);
    pthread_cond_destroy(&q->cond_not_empty);
    pthread_mutex_destroy(&q->mutex);
    free(q->buf);
    q->buf = NULL;
}

// On failure, undoes only the steps that succeeded, in reverse order,
// and returns false. The queue must not be used or destroyed after that.
bool bq_init(struct bq *q, size_t cap) {
    if (cap == 0) // put would block forever, and % 0 is UB
        return false;
    q->head = q->tail = q->count = 0;
    q->cap = cap;

    q->buf = malloc(cap * sizeof *q->buf);
    if (!q->buf)
        return false;
    if (pthread_mutex_init(&q->mutex, NULL) != 0)
        goto err_buf;
    if (pthread_cond_init(&q->cond_not_empty, NULL) != 0)
        goto err_mutex;
    if (pthread_cond_init(&q->cond_not_full, NULL) != 0)
        goto err_not_empty;
    return true;

err_not_empty:
    pthread_cond_destroy(&q->cond_not_empty);
err_mutex:
    pthread_mutex_destroy(&q->mutex);
err_buf:
    free(q->buf);
    q->buf = NULL;
    return false;
}
void bq_put(struct bq *q, int v) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == q->cap) { // buffer full
        pthread_cond_wait(&q->cond_not_full, &q->mutex);
    }
    q->buf[q->head] = v;
    q->head = (q->head + 1) % q->cap;
    q->count++;
    pthread_cond_signal(&q->cond_not_empty);
    pthread_mutex_unlock(&q->mutex);
}

int bq_get(struct bq *q) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0) { // buffer empty
        pthread_cond_wait(&q->cond_not_empty, &q->mutex);
    }
    int val = q->buf[q->tail];
    q->tail = (q->tail + 1) % q->cap;
    q->count--;
    pthread_cond_signal(&q->cond_not_full);
    pthread_mutex_unlock(&q->mutex);
    return val;
}

struct worker {
    struct bq *q;
    int id;    // id of the producer
    int *seen; // where to record what it has gotten.
    size_t n;  // how many items to put or get
};

void *producer(void *args) {
    struct worker *arg = (struct worker *)args;
    for (size_t i = 0; i < arg->n; i++) {
        bq_put(arg->q, arg->id * arg->n + i);
    }
    return NULL;
}

void *consumer(void *args) {
    struct worker *arg = (struct worker *)args;
    for (size_t i = 0; i < arg->n; i++) {
        arg->seen[bq_get(arg->q)] += 1;
    }
    return NULL;
}
int main(void) {
    size_t N = 100000; // How many the producers produce
    struct bq q;
    if (!bq_init(&q, 4)) {
        fprintf(stderr, "cannot create the buffer\n");
        exit(1);
    }
    pthread_t writers[3];
    pthread_t readers[3];
    int *seen = calloc(3 * N, sizeof(int)); // each writer writes 3 times
    struct worker writer_args[3];
    struct worker reader_args[3];
    for (int i = 0; i < 3; i++) {
        writer_args[i].q = &q;
        writer_args[i].id = i;
        writer_args[i].seen = seen;
        writer_args[i].n = N;
        int ret = pthread_create(&writers[i], NULL, producer, &writer_args[i]);
        if (ret != 0) {
            bq_destroy(&q);
            fprintf(stderr, "creation of thread failed\n");
            free(seen);
            exit(1);
        }
    }
    for (int i = 0; i < 3; i++) {
        reader_args[i].q = &q;
        reader_args[i].id = i;
        reader_args[i].seen = seen;
        reader_args[i].n = N;
        int ret = pthread_create(&readers[i], NULL, consumer, &reader_args[i]);
        if (ret != 0) {
            bq_destroy(&q);
            fprintf(stderr, "creation of thread failed\n");
            free(seen);
            exit(1);
        }
    }
    for (int i = 0; i < 3; i++) {
        if (pthread_join(writers[i], NULL) != 0) {
            free(seen);
            bq_destroy(&q);
            exit(1);
        }
    }
    for (int i = 0; i < 3; i++) {
        if (pthread_join(readers[i], NULL) != 0) {
            free(seen);
            bq_destroy(&q);
            exit(1);
        }
    }
    for (size_t i = 0; i < 3 * N; i++) {
        assert(seen[i] == 1);
    }
    free(seen);
    bq_destroy(&q);
    return 0;
}
