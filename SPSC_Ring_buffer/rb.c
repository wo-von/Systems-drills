#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct Ring {
    uint64_t max_capacity;
    uint64_t *readptr;
    uint64_t *writeptr;
    uint64_t *start;
    uint64_t members; // how much is full?
};

void init_ring(struct Ring *ring) {
    if (ring->max_capacity == 0) {
        return;
    }
    ring->start = malloc(ring->max_capacity * sizeof(uint64_t));
    ring->readptr = ring->start;
    ring->writeptr = ring->start;
    ring->members = 0;
}

void kill_ring(struct Ring *ring) {
    free(ring->start);
}

// returns the nearest power of 2 of the user input
int get_capacity(int c) {
    int count = 0;
    while (c > 0) {
        count++;
        c >>= c;
    }
    return count;
}

int main(int argc, char *argv[]) {
    uint32_t capacity = 0;
    if (argc == 1) {
        capacity = 1UL << 10; // 1024
    } else if (argc == 2) {
        capacity = get_capacity(atoi(argv[1]));
        if (capacity <= 0) {
            fprintf(stderr, "capacity must be a positive int\n");
            return 1;
        }
    } else if (argc > 2) {
        fprintf(stderr, "usage rb [size]\n");
    }
    struct Ring ring;
    ring.max_capacity = capacity;
    init_ring(&ring);
}