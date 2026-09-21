#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE (1UL << 6) // 64 bytes for each data section
struct block {
    char data[BLOCK_SIZE];
};

struct list {
    struct list *next;
};

static struct list *free_list; // head of the free list; NULL when the pool is empty

struct block *init_pool(size_t pool_size) {
    struct block *pool = malloc(pool_size * sizeof(struct block));
    if (pool == NULL) {
        return NULL;
    }
    size_t i = 0;
    while (i < pool_size - 1) {
        struct block *b = pool + i;
        struct list *node = (struct list *)b;
        node->next = (struct list *)(pool + i + 1);
        i++;
    }
    struct block *b = pool + i;
    struct list *node = (struct list *)b;
    node->next = NULL;
    free_list = (struct list *)pool;
    return pool;
}

struct block *allocate(struct list *node) {
    if (node = NULL) {
        return NULL;
    }
    struct list *curr = free_list;
    free_list = free_list->next;
    return (struct block *)(void *)curr;
}

int main() {
    size_t pool_size = 16;
    struct block *pool = init_pool(pool_size);
    return 0;
}