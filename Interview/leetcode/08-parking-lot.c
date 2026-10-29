#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_TYPES 3

enum Size {
    SMALL,
    MEDIUM,
    LARGE,
};

enum Vehicle {
    MOTORCYCLE,
    CAR,
    TRUCK,
};

struct Ticket {
    size_t level;
    size_t spot;
};

struct Spot {
    bool occupied;
    enum Size size;
};

// Simplification: every level has the same layout (num_small, num_medium, num_large).
struct Lot {
    struct Spot *spots; // levels * spots_floor entries, level by level
    size_t levels;
    size_t spots_floor;
    size_t free_count[NUM_TYPES]; // kept in step with the occupied flags by park/leave only
};

// The fit rule lives in the lot, not in the caller.
static enum Size smallest_fit(enum Vehicle v) {
    switch (v) {
    case MOTORCYCLE:
        return SMALL;
    case CAR:
        return MEDIUM;
    case TRUCK:
        return LARGE;
    }
    return LARGE;
}

// One flat block used as [level][spot].
static struct Spot *spot_at(struct Lot *lot, size_t level, size_t spot) {
    return &lot->spots[level * lot->spots_floor + spot];
}

bool create_lot(struct Lot *lot, size_t levels, size_t num_small, size_t num_medium,
                size_t num_large) {
    size_t per_floor = num_small + num_medium + num_large;
    if (levels == 0 || per_floor == 0) {
        return false;
    }
    lot->spots = calloc(levels * per_floor, sizeof *lot->spots); // all free
    if (lot->spots == NULL) {
        return false;
    }
    lot->levels = levels;
    lot->spots_floor = per_floor;
    for (size_t i = 0; i < levels; i++) {
        for (size_t j = 0; j < per_floor; j++) {
            enum Size s = j < num_small ? SMALL : j < num_small + num_medium ? MEDIUM : LARGE;
            spot_at(lot, i, j)->size = s;
        }
    }
    lot->free_count[SMALL] = levels * num_small;
    lot->free_count[MEDIUM] = levels * num_medium;
    lot->free_count[LARGE] = levels * num_large;
    return true;
}

void destroy_lot(struct Lot *lot) {
    free(lot->spots);
    lot->spots = NULL;
}

// Parks in the smallest free spot that fits, searching size by size across all levels.
// Returns true and fills *ticket, or false if the lot is full for this vehicle.
bool assigner(struct Lot *lot, enum Vehicle v, struct Ticket *ticket) {
    for (enum Size s = smallest_fit(v); s <= LARGE; s++) {
        if (lot->free_count[s] == 0) {
            continue;
        }
        for (size_t i = 0; i < lot->levels; i++) {
            for (size_t j = 0; j < lot->spots_floor; j++) {
                struct Spot *sp = spot_at(lot, i, j);
                if (!sp->occupied && sp->size == s) {
                    sp->occupied = true;
                    lot->free_count[s]--;
                    ticket->level = i;
                    ticket->spot = j;
                    return true;
                }
            }
        }
    }
    return false;
}

// Frees the spot on the ticket. Returns false for an invalid ticket or a spot that is
// already free (double leave).
bool deassigner(struct Lot *lot, const struct Ticket *ticket) {
    if (ticket->level >= lot->levels || ticket->spot >= lot->spots_floor) {
        return false;
    }
    struct Spot *sp = spot_at(lot, ticket->level, ticket->spot);
    if (!sp->occupied) {
        return false;
    }
    sp->occupied = false;
    lot->free_count[sp->size]++;
    return true;
}

size_t free_spots(const struct Lot *lot, enum Size s) {
    return lot->free_count[s];
}

int main(void) {
    struct Lot lot;
    if (!create_lot(&lot, 2, 2, 3, 1)) { // 2 levels, each 2 small, 3 medium, 1 large
        fprintf(stderr, "could not create lot\n");
        exit(1);
    }
    assert(free_spots(&lot, SMALL) == 4);
    assert(free_spots(&lot, MEDIUM) == 6);
    assert(free_spots(&lot, LARGE) == 2);

    // Trucks: only 2 large spots, the third is turned away.
    struct Ticket t1, t2, t3;
    assert(assigner(&lot, TRUCK, &t1));
    assert(assigner(&lot, TRUCK, &t2));
    assert(!assigner(&lot, TRUCK, &t3));
    assert(free_spots(&lot, LARGE) == 0);

    // A truck leaves: its spot can be used again, and a double leave is rejected.
    assert(deassigner(&lot, &t1));
    assert(!deassigner(&lot, &t1));
    assert(assigner(&lot, TRUCK, &t3));
    assert(t3.level == t1.level && t3.spot == t1.spot);

    // Motorcycles take small spots first, then overflow into medium.
    struct Ticket m;
    for (int k = 0; k < 4; k++) {
        assert(assigner(&lot, MOTORCYCLE, &m));
    }
    assert(free_spots(&lot, SMALL) == 0);
    assert(assigner(&lot, MOTORCYCLE, &m));
    assert(free_spots(&lot, MEDIUM) == 5);

    // Cars never take small spots: 5 medium left, the sixth car is turned away.
    struct Ticket c;
    for (int k = 0; k < 5; k++) {
        assert(assigner(&lot, CAR, &c));
    }
    assert(!assigner(&lot, CAR, &c));

    // Invalid ticket.
    struct Ticket bad = {.level = 7, .spot = 0};
    assert(!deassigner(&lot, &bad));

    destroy_lot(&lot);
    puts("all tests passed");
    return 0;
}
