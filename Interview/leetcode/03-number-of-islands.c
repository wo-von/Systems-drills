#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int dir_x[4] = {-1, 0, 1, 0};
const int dir_y[4] = {0, 1, 0, -1};

struct Queue {
    int *x;
    int *y;
    int head, tail;
};

void queue_add(struct Queue *q, int x, int y) {
    q->x[q->head] = x;
    q->y[q->head] = y;
    (q->head)++;
}

int numIslands(char **grid, int gridSize, int *gridColSize) {
    int island_count = 0;
    struct Queue qu;
    qu.x = malloc((size_t)gridSize * gridColSize[0] * sizeof(*qu.x));
    qu.y = malloc((size_t)gridSize * gridColSize[0] * sizeof(*qu.y));
    if (!qu.x || !qu.y) {
        fprintf(stderr, "malloc failed\n");
        free(qu.x);
        free(qu.y);
        return -1;
    }
    qu.head = qu.tail = 0;
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridColSize[0]; j++) {
            if (grid[i][j] == '0') {
                continue;
            }
            grid[i][j] = '0';
            island_count++;
            queue_add(&qu, i, j);
            while (qu.tail != qu.head) {
                int this_x = qu.x[qu.tail];
                int this_y = qu.y[qu.tail];
                qu.tail++;
                for (int k = 0; k < 4; k++) {
                    int next_x = this_x + dir_x[k];
                    int next_y = this_y + dir_y[k];
                    if (next_x < 0 || next_x >= gridSize || next_y < 0 ||
                        next_y >= gridColSize[0]) {
                        continue;
                    }
                    if (grid[next_x][next_y] == '1') {
                        grid[next_x][next_y] = '0';
                        queue_add(&qu, next_x, next_y);
                    }
                }
            }
        }
    }
    free(qu.x);
    free(qu.y);
    return island_count;
}

int main(void) {
    {
        char r0[] = "1100";
        char r1[] = "1100";
        char r2[] = "0010";
        char r3[] = "0010";
        char *grid[] = {r0, r1, r2, r3};
        int gridSize = 4;
        int gridSizeCol[] = {4, 4, 4, 4};
        int num = numIslands(grid, gridSize, gridSizeCol);
        assert(num == 2);
    }
    {
        char r0[] = "10101";
        char r1[] = "01010";
        char r2[] = "10101";
        char r3[] = "01010";
        char *grid[] = {r0, r1, r2, r3};
        int gridSize = 4;
        int gridSizeCol[] = {5, 5, 5, 5};
        int num = numIslands(grid, gridSize, gridSizeCol);
        assert(num == 10);
    }
    {
        char r0[] = "1111";
        char r1[] = "1111";
        char r2[] = "1111";
        char r3[] = "1111";
        char *grid[] = {r0, r1, r2, r3};
        int gridSize = 4;
        int gridSizeCol[] = {4, 4, 4, 4};
        int num = numIslands(grid, gridSize, gridSizeCol);
        assert(num == 1);
    }
    {
        char r0[] = "1100";
        char r1[] = "1100";
        char r2[] = "0010";
        char r3[] = "0010";
        char *grid[] = {r0, r1, r2, r3};
        int gridSize = 4;
        int gridSizeCol[] = {4, 4, 4, 4};
        int num = numIslands(grid, gridSize, gridSizeCol);
        assert(num == 2);
    }
    {
        char r0[] = "1";
        char r1[] = "1";
        char r2[] = "1";
        char r3[] = "1";
        char *grid[] = {r0, r1, r2, r3};
        int gridSize = 4;
        int gridSizeCol[] = {1, 1, 1, 1};
        int num = numIslands(grid, gridSize, gridSizeCol);
        assert(num == 1);
    }
    return 0;
}
