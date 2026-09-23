#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct worker_arg {
    long int iter;
    volatile long long int *counter;
};

/*
The compiler does not loop! so race could not be seen without the guard
that is why counter is volatile
0000000000001280 < worker > :
1280 : mov(% rdi), % rax
1283 : test % rax, % rax
1286 : jle 128f < worker + 0xf >
1288 : mov 0x8(% rdi), % rdx
128c : add % rax, (% rdx)
128f : xor % eax, % eax
1291 : ret
*/
void *worker(void *args) {
    struct worker_arg *arg = args;
    for (int i = 0; i < arg->iter; i++) {
        (*(arg->counter))++;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage visible_race [reps]\n");
        return 1;
    }
    pthread_t thds[4];

    struct worker_arg args;
    long long int c = 0;
    args.iter = atol(argv[1]);
    args.counter = &c;

    for (int i = 0; i < 4; i++) {
        pthread_create(&thds[i], NULL, &worker, &args);
    }

    for (int i = 0; i < 4; i++) {
        if (pthread_join(thds[i], NULL) != 0) {
            perror("thread");
        }
    }
    printf("counter is %lld\n", *(args.counter));

    return 0;
}
