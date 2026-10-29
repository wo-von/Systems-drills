#include<string.h>
#include<stdbool.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>

struct Queue {
    int *row;
    int *col;
    size_t head, tail;
};

const int dir_x[4] = {-1 , 0, 1, 0};
const int dir_y[4] = {0, 1, 0, -1};

void queue_put(struct Queue *q, int x, int y){
    q->row[q->head] = x;
    q->col[q->head] = y;
    q->head++;
    return;
}

void queue_get(struct Queue *q, int *x, int *y){
    *x = q->row[q->tail];
    *y = q->col[q->tail];
    q->tail++;
}

bool queue_empty(struct Queue *q){
    return q->head == q->tail;
}

int numberOfIslands(char **grid, int rows, int *cols){
    int num_islands = 0;
    struct Queue q;
    q.head = 0;
    q.tail = 0;
    q.row = malloc(rows);
    if (q.row == NULL){
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    q.col = malloc(cols[0]);
    if (q.col == NULL){
        fprintf(stderr, "malloc failed\n");
        free(q.row);
        exit(1);
    }
    for (int i = 0; i < rows; i++){
        for (int j = 0; j < cols[0]; j++){
            if (grid[i][j] == '0') {
                continue;
            }
            queue_put(&q, i, j);
            grid[i][j] = '0';
            num_islands++;
            while (!queue_empty(&q)){
                int this_x, this_y;
                queue_get(&q, &this_x, &this_y);
                for (int k = 0; k < 4; k++){
                    int next_x = this_x + dir_x[k];
                    int next_y = this_y + dir_y[k];
                    if (next_x < 0 || next_x >= rows || next_y < 0 || next_y >= cols[0]
                            || grid[next_x][next_y] == '0'){
                        continue;
                    }
                    queue_put(&q, next_x, next_y);
                    grid[next_x][next_y] = '0';
                }
            }
        }
    }
    free(q.row);
    free(q.col);
    return num_islands;
}
int main(void){
    {
        char r1[] = "1100";
        char r2[] = "1100";
        char r3[] = "0010";
        char r4[] = "0001";
        char *grid[] = {r1, r2, r3, r4};
        int cols[] = {4, 4, 4, 4};
        assert (numberOfIslands(grid, 4, cols) == 3);
    }
    return 0;
}
