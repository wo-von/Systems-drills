#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define NUM_REGIONS (1UL << 9)

enum register_codes {
    REGISTER_SUCCESS = 0,
    REGION_OVERLAP,
    SIZE_ZERO,
    ADDRESS_OVERFLOW,
    REGIONS_FULL,
};

struct region {
    uint64_t guest_physical_start;
    uint64_t memsize;
    uint64_t host_virtual;
};

struct region_table {
    struct region regions[NUM_REGIONS];
    size_t registered; // regions[0 .. registered-1] are valid
};

// registers a region in the table; regions are half-open [start, start + memsize)
enum register_codes region_add(struct region_table *tbl, const struct region *rgn) {
    if (rgn->memsize == 0)
        return SIZE_ZERO;
    // start + memsize must not wrap, in either address space
    if (UINT64_MAX - rgn->memsize < rgn->guest_physical_start)
        return ADDRESS_OVERFLOW;
    if (UINT64_MAX - rgn->memsize < rgn->host_virtual)
        return ADDRESS_OVERFLOW;
    if (tbl->registered == NUM_REGIONS)
        return REGIONS_FULL;

    uint64_t new_start = rgn->guest_physical_start;
    uint64_t new_end = new_start + rgn->memsize;
    for (size_t i = 0; i < tbl->registered; i++) {
        const struct region *old = &tbl->regions[i];
        uint64_t old_end = old->guest_physical_start + old->memsize; // checked when it was added
        // disjoint: new ends at or before old starts, or new starts at or after old ends
        if (!(new_end <= old->guest_physical_start || new_start >= old_end))
            return REGION_OVERLAP;
    }
    // first free slot is at index registered
    tbl->regions[tbl->registered] = *rgn;
    tbl->registered++;
    return REGISTER_SUCCESS;
}

// on success writes the host virtual address to *hva and returns true;
// returns false if gpa is not inside any registered region
bool translate(const struct region_table *tbl, uint64_t gpa, uint64_t *hva) {
    for (size_t i = 0; i < tbl->registered; i++) {
        const struct region *r = &tbl->regions[i];
        // gpa >= start makes gpa - start safe; no start + memsize, so nothing can wrap
        if (gpa >= r->guest_physical_start && gpa - r->guest_physical_start < r->memsize) {
            *hva = r->host_virtual + (gpa - r->guest_physical_start);
            return true;
        }
    }
    return false;
}

int main(void) {
    static struct region_table tbl; // static: zero-initialized, and 12 KiB off the stack
    uint64_t hva;

    // A = [0x1000, 0x2000)
    const struct region a = {0x1000, 0x1000, 0x7f0000000000};
    assert(region_add(&tbl, &a) == REGISTER_SUCCESS);

    // translate at the edges of A
    assert(translate(&tbl, 0x1000, &hva) && hva == 0x7f0000000000);
    assert(translate(&tbl, 0x1fff, &hva) && hva == 0x7f0000000fff);
    assert(!translate(&tbl, 0x2000, &hva));
    assert(!translate(&tbl, 0x0fff, &hva));

    // overlap cases against A
    assert(region_add(&tbl, &(struct region){0x1800, 0x1000, 0x7f0000100000}) == REGION_OVERLAP);
    assert(region_add(&tbl, &(struct region){0x0000, 0x1800, 0x7f0000100000}) == REGION_OVERLAP);
    assert(region_add(&tbl, &(struct region){0x0000, 0x4000, 0x7f0000100000}) == REGION_OVERLAP);
    assert(region_add(&tbl, &(struct region){0x3000, 0x1000, 0x7f0000100000}) == REGISTER_SUCCESS);
    // adjacent to A on the right: touches, does not overlap
    assert(region_add(&tbl, &(struct region){0x2000, 0x1000, 0x7f0000200000}) == REGISTER_SUCCESS);
    assert(translate(&tbl, 0x2000, &hva) && hva == 0x7f0000200000);

    // invalid registrations
    assert(region_add(&tbl, &(struct region){0x9000, 0, 0x7f0000300000}) == SIZE_ZERO);
    assert(region_add(&tbl, &(struct region){UINT64_MAX - 0xfff, 0x1000, 0x7f0000300000}) ==
           ADDRESS_OVERFLOW);

    printf("all tests passed\n");
    return 0;
}
