#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Map {
    int *keys;
    int *values;
    int cap;
    bool *exist;
    size_t mask;
};

size_t hash_map(struct Map *mp, int index) {
    unsigned long hash = (unsigned long)index;
    hash *= 0x9E3779B1u;
    hash ^= hash >> 16;
    return (size_t)hash & mp->mask;
}

void free_map(struct Map *mp) {
    free(mp->keys);
    mp->keys = NULL;
    free(mp->values);
    mp->values = NULL;
    free(mp->exist);
    mp->exist = NULL;
    mp->cap = 0;
    mp->mask = 0;
}

bool init_map(struct Map *mp, int cap) {
    int c = 1;
    while (c < cap) {
        c <<= 1;
    }
    mp->cap = c;
    mp->mask = c - 1;
    mp->keys = malloc(mp->cap * sizeof(int));
    mp->values = malloc(mp->cap * sizeof(int));
    mp->exist = calloc(mp->cap, sizeof(bool));
    if (!mp->keys || !mp->values || !mp->exist) {
        free_map(mp);
        return false;
    }
    return true;
}

size_t probe_map(struct Map *mp, int key) {
    size_t index = hash_map(mp, key);
    while (mp->exist[index] && mp->keys[index] != key) {
        index = (index + 1) & mp->mask;
    }
    return index;
}

void set_map(struct Map *mp, int key, int value) {
    size_t index = probe_map(mp, key);
    mp->keys[index] = key;
    mp->values[index] = value;
    mp->exist[index] = true;
}

// sets the value in pointer to it if exists and return true
// otherwose returns false
bool get_map(struct Map *mp, int key, int *value) {
    size_t index = probe_map(mp, key);
    if (mp->exist[index]) {
        *value = mp->values[index];
        return true;
    }
    return false;
}

int *twoSum(int *nums, int numsSize, int target, int *returnSize) {
    struct Map mp;
    int *res = malloc(2 * sizeof(int));
    *returnSize = 0;
    if (!res) {
        return NULL;
    }
    if (!init_map(&mp, 2 * numsSize)) {
        fprintf(stderr, "cannot initialize map\n");
        free(res);
        return NULL;
    }
    for (int i = 0; i < numsSize; i++) {
        int want = target - nums[i];
        int pair;
        if (get_map(&mp, want, &pair)) {
            res[0] = pair, res[1] = i;
            *returnSize = 2;
            free_map(&mp);
            return res;
        }
        set_map(&mp, nums[i], i);
    }
    free_map(&mp);
    free(res);
    return NULL;
}

int main(void) {
    {
        int nums[2] = {3, 3};
        int returnSize;
        int *res = twoSum(nums, 2, 6, &returnSize);
        assert(res[0] == 0);
        assert(res[1] == 1);
        assert(returnSize == 2);
        free(res);
    }
    {
        int nums[4] = {2, 7, 11, 15};
        int returnSize;
        int *res = twoSum(nums, 4, 9, &returnSize);
        assert(res[0] == 0);
        assert(res[1] == 1);
        assert(returnSize == 2);
        free(res);
    }
    {
        int nums[4] = {-3, 4, 3, 90};
        int returnSize;
        int *res = twoSum(nums, 4, 0, &returnSize);
        assert(res[0] == 0);
        assert(res[1] == 2);
        assert(returnSize == 2);
        free(res);
    }
    return 0;
}
