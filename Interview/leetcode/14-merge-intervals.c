#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct Interval {
    int start, end;
};

int cmp(const void *a, const void *b) {
    const struct Interval *x = a, *y = b; /* or const struct Interval * */
    return (x->start > y->start) - (x->start < y->start);
}

struct Interval *merge(struct Interval *iv, size_t n, size_t *out_n) {
    if (n == 0) {
        *out_n = n;
        return NULL;
    }
    qsort(iv, n, sizeof(struct Interval), cmp);
    struct Interval *head = malloc(n * sizeof(struct Interval));
    if (head == NULL) {
        fprintf(stderr, "malloc failed\n");
        return NULL;
    }
    struct Interval *ptr = head;
    *ptr = iv[0];
    for (size_t i = 1; i < n; i++) {
        if (iv[i].start <= ptr->end) {
            if (iv[i].end < ptr->end) {
                continue;
            } else {
                ptr->end = iv[i].end;
            }
        } else {
            *++ptr = iv[i];
        }
    }
    *out_n = ptr - head + 1;
    return head;
}

int main(void) {
    {
        struct Interval iv[] = {{1, 3}, {2, 6}, {8, 10}, {15, 18}};
        size_t m;
        struct Interval *res = merge(iv, sizeof iv / sizeof *iv, &m);
        assert(res != NULL);
        struct Interval sentinel[] = {{1, 6}, {8, 10}, {15, 18}};
        assert(m == (sizeof(sentinel) / sizeof(*sentinel)));
        for (size_t i = 0; i < sizeof sentinel / sizeof *sentinel; i++) {
            assert(res[i].start == sentinel[i].start && res[i].end == sentinel[i].end);
        }
        free(res);
    }
    {
        struct Interval iv[] = {{1, 4}, {2, 3}};
        size_t m;
        struct Interval *res = merge(iv, sizeof iv / sizeof *iv, &m);
        assert(res != NULL);
        struct Interval sentinel[] = {{1, 4}};
        assert(m == (sizeof(sentinel) / sizeof(*sentinel)));
        for (size_t i = 0; i < m; i++) {
            assert(res[i].start == sentinel[i].start && res[i].end == sentinel[i].end);
        }
        free(res);
    }
}
