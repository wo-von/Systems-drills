#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int dir_x[4] = {-1, 0, 1, 0};
const int dir_y[4] = {0, 1, 0, -1};

struct Queue {
    int buff[100][2]; // grid is 10x10
    size_t tail, head;
};

void queue_push(struct Queue *q, int x, int y) {
    q->buff[q->head][0] = x;
    q->buff[q->head][1] = y;
    q->head++;
}

int *queue_get(struct Queue *q) {
    if (q->head == q->tail) {
        return NULL;
    }
    q->tail++;
    return q->buff[q->tail - 1];
}

void queue_swap(struct Queue **a, struct Queue **b) {
    struct Queue *tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

bool queue_empty(struct Queue *q) {
    return q->head == q->tail;
}

int orangesRotting(int **grid, int gridSize, int *gridColSize) {
    int to_rott = 0, time = 0;
    struct Queue first = {0};
    struct Queue second = {0};
    struct Queue *curr, *next;
    curr = &first;
    next = &second;
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridColSize[0]; j++) {
            if (grid[i][j] == 1) {
                to_rott++;
            } else if (grid[i][j] == 2) {
                queue_push(next, i, j);
            }
        }
    }
    while (!queue_empty(next) && (to_rott != 0)) {
        time++;
        queue_swap(&curr, &next);
        while (!queue_empty(curr)) {
            int *this_xy = queue_get(curr);
            int this_x = this_xy[0], this_y = this_xy[1];
            for (int k = 0; k < 4; k++) {
                int next_x = this_x + dir_x[k], next_y = this_y + dir_y[k];
                if (next_x < 0 || next_x >= gridSize || next_y < 0 || next_y >= gridColSize[0]) {
                    continue;
                }
                if (grid[next_x][next_y] == 1) {
                    grid[next_x][next_y] = 2;
                    queue_push(next, next_x, next_y);
                    to_rott--;
                }
            }
        }
    }

    return to_rott > 0 ? -1 : time;
}

int main(void) {
    {
        int r0[] = {2, 1, 1};
        int r1[] = {1, 1, 0};
        int r2[] = {0, 1, 1};
        int *gr[] = {r0, r1, r2};
        int gridSize = 3;
        int colsize[] = {3, 3, 3};
        int time = orangesRotting(gr, gridSize, colsize);
        assert(time == 4);
    }
    {
        int r0[] = {2, 1, 1};
        int r1[] = {0, 1, 1};
        int r2[] = {1, 0, 1};
        int *gr[] = {r0, r1, r2};
        int gridSize = 3;
        int colsize[] = {3, 3, 3};
        int time = orangesRotting(gr, gridSize, colsize);
        assert(time == -1);
    }
    {
        int r0[] = {0, 2};
        int *gr[] = {r0};
        int gridSize = 1;
        int colsize[] = {2};
        int time = orangesRotting(gr, gridSize, colsize);
        assert(time == 0);
    }
    return 0;
}
