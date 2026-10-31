#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

bool can_ship(const int *weights, int n, int days, int cap) {
    int load = 0;
    int used = 1;
    for (int i = 0; i < n; i++) {
        if (weights[i] > cap)
            return false;
        if (load + weights[i] > cap) {
            load = 0, used++;
        }
        load += weights[i];
    }
    return used <= days;
}

int ship_within_days(const int *weights, int n, int days) {
    int lo = weights[0], hi = 0, mid = 0;
    for (int i = 0; i < n; i++) {
        hi += weights[i];
        if (weights[i] > lo) {
            lo = weights[i];
        }
    }
    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        if (can_ship(weights, n, days, mid)) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

int main(void) {
    {
        int w[] = {1, 2, 3, 4, 5};
        int cap = ship_within_days(w, sizeof w / sizeof *w, 3);
        assert(cap == 6);
    }
    {
        int w[] = {6};
        assert(ship_within_days(w, sizeof w / sizeof *w, 1) == 6);
    }
}
