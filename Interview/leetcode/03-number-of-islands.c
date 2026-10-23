#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// must be looped with same indices
const int dir_x[4] = {-1, 0, 1, 0};
const int dir_y[4] = {0, 1, 0, -1};

struct Queue {
    int buffer[90000][2]; // should be larger than the grid
    size_t tail, head;
};

void queue_push(struct Queue *q, int to_push_x, int to_push_y) {
    q->buffer[q->head][0] = to_push_x;
    q->buffer[q->head][1] = to_push_y;
    q->head++;
    return;
}

int *queue_read(struct Queue *q){
    q->tail++;
    return q->buffer[q->tail - 1];
}

bool queue_empty(struct Queue *q) {
    return q->head == q->tail;
}

int numIslands(char **grid, int gridSize, int *gridColSize) {
    int num_islands = 0;
    if (grid == NULL) {
        return num_islands;
    }
    struct Queue queue = {0};
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridColSize[0]; j++) {
            char cell = grid[i][j];
            if (cell == '0') {
                continue;
            }
            queue_push(&queue, i, j);
            num_islands++;
            grid[i][j] = '0';
            while (!queue_empty(&queue)){
                int *next_arr = queue_read(&queue);
                int next_x = next_arr[0], next_y = next_arr[1];
                for (int k = 0; k < 4; k++){
                    int this_x = next_x + dir_x[k];
                    int this_y = next_y + dir_y[k];
                    if (this_x < 0 || this_y < 0 || this_x >= gridSize || this_y >= gridColSize[0]){
                        continue;
                    }
                    if (grid[this_x][this_y] == '0'){
                        continue;
                    }
                    grid[this_x][this_y] = '0';
                    queue_push(&queue, this_x, this_y);
                    }
                }
            
            }
        }
        return num_islands;
    }

int main(void){
    {
        char r0[] = "11000", r1[] = "11000", r2[] = "00100", r3[] = "00011";
        char *g[] = {r0, r1, r2, r3};
        int cols[] = {5, 5, 5, 5};
        int res = numIslands(g, 4, cols);
        assert(res == 3);
    }
    {
        char r0[] = "11111", r1[] = "11111", r2[] = "11111", r3[] = "11111";
        char *g[] = {r0, r1, r2, r3};
        int cols[] = {5, 5, 5, 5};
        int res = numIslands(g, 4, cols);
        assert(res == 1);
    }
    {
        char r0[] = "10000", r1[] = "00000", r2[] = "00000", r3[] = "00000";
        char *g[] = {r0, r1, r2, r3};
        int cols[] = {5, 5, 5, 5};
        int res = numIslands(g, 4, cols);
        assert(res == 1);
    }
    {
        char r0[] = "10101", r1[] = "10101", r2[] = "10101", r3[] = "10101";
        char *g[] = {r0, r1, r2, r3};
        int cols[] = {5, 5, 5, 5};
        int res = numIslands(g, 4, cols);
        assert(res == 3);
    }
    {
        char r0[] = "10101", r1[] = "01010", r2[] = "10101", r3[] = "01010";
        char *g[] = {r0, r1, r2, r3};
        int cols[] = {5, 5, 5, 5};
        int res = numIslands(g, 4, cols);
        assert(res == 10);
    }
    {
        char r0[] = "00000", r1[] = "00000", r2[] = "00000", r3[] = "00000";
        char *g[] = {r0, r1, r2, r3};
        int cols[] = {5, 5, 5, 5};
        int res = numIslands(g, 4, cols);
        assert(res == 0);
    }
    return 0;
}

