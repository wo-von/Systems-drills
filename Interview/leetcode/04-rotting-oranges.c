#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dir_x[4] = {-1, 0, 1, 0};
int dir_y[4] = {0, 1, 0, -1};

struct Pair {
    int x;
    int y;
};
struct Queue {
    struct Pair buff[100]; // max 100 entries is enough since m,n<=10
    int head, tail;
};

void queue_add(struct Queue *q, int x, int y) {
    q->buff[q->head].x = x;
    q->buff[q->head].y = y;
    q->head++;
}

int orangesRotting(int **grid, int gridSize, int *gridColSize) {
    int fresh = 0;
    struct Queue first = {0};
    struct Queue second = {0};
    struct Queue *curr = &first;
    struct Queue *next = &second;
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridColSize[0]; j++) {
            if (grid[i][j] == 1) {
                fresh++;
            } else if (grid[i][j] == 2) {
                queue_add(next, i, j);
            }
        }
    }
    int minutes = 0;
    while (next->head != next->tail && fresh) {
        minutes++;
        struct Queue *tmp = next;
        next = curr;
        curr = tmp;
        int this_x, this_y;
        while (curr->head != curr->tail) {
            this_x = curr->buff[curr->tail].x;
            this_y = curr->buff[curr->tail].y;
            curr->tail++;
            for (int k = 0; k < 4; k++) {
                int next_x = this_x + dir_x[k];
                int next_y = this_y + dir_y[k];
                if (next_x < 0 || next_x >= gridSize || next_y < 0 || next_y >= gridColSize[0]) {
                    continue;
                }
                if (grid[next_x][next_y] == 1) {
                    fresh--;
                    queue_add(next, next_x, next_y);
                    grid[next_x][next_y] = 2;
                }
            }
        }
    }
    return fresh ? -1 : minutes;
}

int main(void) {
    {
        int r0[] = {2, 1, 0, 0};
        int r1[] = {1, 1, 1, 0};
        int r2[] = {0, 0, 1, 0};
        int r3[] = {0, 0, 1, 0};
        int *grid[] = {r0, r1, r2, r3};
        int gridSize = 4;
        int gridColSize[4] = {4, 4, 4, 4};
        int res = orangesRotting(grid, gridSize, gridColSize);
        assert(res == 5);
    }
    {
        int r0[] = {2, 1, 1};
        int r1[] = {1, 1, 0};
        int r2[] = {0, 1, 1};
        int *grid[] = {r0, r1, r2};
        int gridSize = 3;
        int gridColSize[] = {3, 3, 3};
        int res = orangesRotting(grid, gridSize, gridColSize);
        assert(res == 4);
    }
    {
        int r0[] = {2, 1, 1};
        int r1[] = {0, 1, 1};
        int r2[] = {1, 0, 1};
        int *grid[] = {r0, r1, r2};
        int gridSize = 3;
        int gridColSize[] = {3, 3, 3};
        int res = orangesRotting(grid, gridSize, gridColSize);
        assert(res == -1);
    }
    {
        int r0[] = {0, 2};
        int *grid[] = {r0};
        int gridSize = 1;
        int gridColSize[] = {2};
        int res = orangesRotting(grid, gridSize, gridColSize);
        assert(res == 0);
    }
    {
        int r0[] = {2, 1, 1, 1, 2};
        int *grid[] = {r0};
        int gridSize = 1;
        int gridColSize[] = {5};
        int res = orangesRotting(grid, gridSize, gridColSize);
        assert(res == 2);
    }
    {
        int r0[] = {1};
        int *grid[] = {r0};
        int gridSize = 1;
        int gridColSize[] = {1};
        int res = orangesRotting(grid, gridSize, gridColSize);
        assert(res == -1);
    }
    {
        int r0[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r1[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r2[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r3[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r4[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r5[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r6[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r7[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r8[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int r9[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
        int *grid[] = {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9};
        int gridSize = 10;
        int gridColSize[] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
        int res = orangesRotting(grid, gridSize, gridColSize);
        assert(res == 18);
    }

    return 0;
}
