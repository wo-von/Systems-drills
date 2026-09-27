// gpa -> hva lookup, lock-free for readers (vCPUs), copy-on-write for the rare writer (VMM).
// A hand-rolled mini-RCU: readers never lock or retry; the writer builds a new table,
// publishes it with one pointer store, waits until no reader can still hold the old
// table, then frees it. Only adds are supported.
//
// build:  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -pthread
//         gcc -std=c11 -O1 -g -fsanitize=thread -pthread
// break:  add -DBREAK_IT to skip the wait, and ASan reports a heap-use-after-free
#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REGIONS 512
#define MAX_READERS 8

struct region {
    uint64_t gpa_start;
    uint64_t size;
    uint64_t hva;
};

// never modified after it is published: readers see all of it or none of it
struct region_table {
    size_t n;
    struct region r[]; // sorted by gpa_start, no overlaps
};

static _Atomic(struct region_table *) current;
static pthread_mutex_t writer_lock = PTHREAD_MUTEX_INITIALIZER; // writers are rare: serialize them

// one per reader (vCPU), each on its own cache line so readers don't slow each other down
struct reader_state {
    _Alignas(64) atomic_ulong seq; // odd while inside a read section; only its reader writes it
};
static struct reader_state readers[MAX_READERS];

// ---- reader side: two stores and a load around the lookup, no lock, no retry ----

static const struct region_table *read_begin(int id) {
    atomic_ulong *seq = &readers[id].seq;
    unsigned long s = atomic_load_explicit(seq, memory_order_relaxed); // my own variable
    // seq_cst on this store and on the load below: the writer does the mirror image
    // (store current, then load seq). With all four seq_cst, the two sides cannot both
    // miss each other's store (store buffering). Acquire/release alone would allow that.
    atomic_store_explicit(seq, s + 1, memory_order_seq_cst);
    return atomic_load_explicit(&current, memory_order_seq_cst); // also acquire: table contents
}

static void read_end(int id) {
    atomic_ulong *seq = &readers[id].seq;
    unsigned long s = atomic_load_explicit(seq, memory_order_relaxed);
    // release: all my reads of the table happen before the writer sees me leave
    atomic_store_explicit(seq, s + 1, memory_order_release);
}

// binary search in a sorted, immutable table
static bool lookup(const struct region_table *t, uint64_t gpa, uint64_t *hva) {
    size_t lo = 0, hi = t->n;
    while (lo < hi) { // find how many regions start at or below gpa
        size_t mid = lo + (hi - lo) / 2;
        if (t->r[mid].gpa_start <= gpa)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == 0)
        return false; // gpa is below the first region
    const struct region *r = &t->r[lo - 1];
    if (gpa - r->gpa_start >= r->size)
        return false; // in a gap after r
    *hva = r->hva + (gpa - r->gpa_start);
    return true;
}

bool translate(int vcpu, uint64_t gpa, uint64_t *hva) {
    const struct region_table *t = read_begin(vcpu);
    bool ok = lookup(t, gpa, hva);
    read_end(vcpu);
    return ok; // t must not be touched after read_end: it may be freed
}

// ---- writer side: copy, publish, wait, free ----

// wait until every reader that might hold the old table has left its read section
static void wait_for_readers(void) {
    for (int i = 0; i < MAX_READERS; i++) {
        unsigned long s = atomic_load_explicit(&readers[i].seq, memory_order_seq_cst);
        if (!(s & 1))
            continue; // not inside: its next read_begin sees the new table
        // inside a section that may have started before the publish: wait for it to end.
        // Comparing to s (not waiting for "even") means a reader that re-enters at once
        // can't starve the writer.
        while (atomic_load_explicit(&readers[i].seq, memory_order_acquire) == s)
            ;
    }
}

enum add_result { ADD_OK, ADD_BAD_SIZE, ADD_WRAPS, ADD_OVERLAP, ADD_FULL, ADD_NOMEM };

enum add_result region_add(const struct region *rgn) {
    if (rgn->size == 0)
        return ADD_BAD_SIZE;
    if (UINT64_MAX - rgn->size < rgn->gpa_start || UINT64_MAX - rgn->size < rgn->hva)
        return ADD_WRAPS;

    pthread_mutex_lock(&writer_lock);
    // relaxed: only writers store current, and the mutex orders us after the last one
    struct region_table *old = atomic_load_explicit(&current, memory_order_relaxed);
    enum add_result res = ADD_OK;

    if (old->n == MAX_REGIONS) {
        res = ADD_FULL;
        goto out;
    }
    size_t pos = 0; // insertion point: first region starting at or after the new one
    while (pos < old->n && old->r[pos].gpa_start < rgn->gpa_start)
        pos++;
    // sorted and disjoint, so only the two neighbours can overlap the new region
    if (pos > 0 && old->r[pos - 1].gpa_start + old->r[pos - 1].size > rgn->gpa_start) {
        res = ADD_OVERLAP;
        goto out;
    }
    if (pos < old->n && old->r[pos].gpa_start < rgn->gpa_start + rgn->size) {
        res = ADD_OVERLAP;
        goto out;
    }

    struct region_table *new = malloc(sizeof *new + (old->n + 1) * sizeof new->r[0]);
    if (new == NULL) {
        res = ADD_NOMEM;
        goto out;
    }
    memcpy(new->r, old->r, pos * sizeof new->r[0]);
    new->r[pos] = *rgn;
    memcpy(new->r + pos + 1, old->r + pos, (old->n - pos) * sizeof new->r[0]);
    new->n = old->n + 1;

    // publish: fully built before the store (release part); seq_cst for wait_for_readers
    atomic_store_explicit(&current, new, memory_order_seq_cst);
#ifndef BREAK_IT
    wait_for_readers();
#endif
    free(old); // no reader can hold it any more

out:
    pthread_mutex_unlock(&writer_lock);
    return res;
}

// ---- test: vCPUs translate while the VMM keeps adding regions ----

#define BASE_HVA 0x7f0000000000ULL
#define CHUNK 0x100000ULL // 1 MiB per region
#define NUM_ADDS 300
#define NUM_VCPUS 4

static atomic_bool stop;

struct vcpu_arg {
    int id;
    long hits, misses;
};

static void *vcpu(void *p) {
    struct vcpu_arg *a = p;
    uint64_t x = 0x9e3779b97f4a7c15ULL * (uint64_t)(a->id + 1); // xorshift state
    while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        uint64_t gpa = x % (CHUNK * (NUM_ADDS + 2)); // sometimes beyond the last region
        uint64_t hva;
        if (translate(a->id, gpa, &hva)) {
            assert(hva == BASE_HVA + gpa); // every region maps gpa to BASE_HVA + gpa
            a->hits++;
        } else {
            assert(gpa >= CHUNK); // region 0 is there from the start, so never a miss
            a->misses++;
        }
    }
    return NULL;
}

int main(void) {
    struct region_table *empty = malloc(sizeof *empty);
    assert(empty != NULL);
    empty->n = 0;
    atomic_store(&current, empty);

    assert(region_add(&(struct region){0, CHUNK, BASE_HVA}) == ADD_OK);
    assert(region_add(&(struct region){CHUNK / 2, CHUNK, BASE_HVA + CHUNK / 2}) == ADD_OVERLAP);

    pthread_t thds[NUM_VCPUS];
    struct vcpu_arg args[NUM_VCPUS];
    for (int i = 0; i < NUM_VCPUS; i++) {
        args[i] = (struct vcpu_arg){.id = i};
        if (pthread_create(&thds[i], NULL, vcpu, &args[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }

    // add regions in a scrambled order so inserts land in the middle of the table too
    for (uint64_t k = 0; k < NUM_ADDS; k++) {
        uint64_t i = 1 + (k * 7) % NUM_ADDS; // 7 is coprime to 300: every i in 1..300 once
        struct region r = {i * CHUNK, CHUNK, BASE_HVA + i * CHUNK};
        assert(region_add(&r) == ADD_OK);
    }

    atomic_store(&stop, true);
    long hits = 0, misses = 0;
    for (int i = 0; i < NUM_VCPUS; i++) {
        pthread_join(thds[i], NULL);
        hits += args[i].hits;
        misses += args[i].misses;
    }

    uint64_t hva;
    for (uint64_t i = 0; i <= NUM_ADDS; i++) // all 301 regions, first and last byte
        assert(translate(0, i * CHUNK, &hva) && translate(0, i * CHUNK + CHUNK - 1, &hva));
    assert(!translate(0, (NUM_ADDS + 1) * CHUNK, &hva));

    printf("ok: %d regions, %ld hits, %ld misses\n", NUM_ADDS + 1, hits, misses);
    free(atomic_load(&current));
    return 0;
}
