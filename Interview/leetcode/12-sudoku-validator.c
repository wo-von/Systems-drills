#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_N 1000

bool sudoku_valid(const int *board, int n, int box) {
    if (n < 0 || n > MAX_N || box <= 0 || box > n || box * box != n) {
        return false;
    }
    bool seen[MAX_N + 1] = {0};
    // check rows
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (board[i * n + j] == 0) {
                continue;
            }
            if (board[i * n + j] < 0 || board[i * n + j] > n) {
                return false;
            }
            if (seen[board[i * n + j]] == 1) {
                return false;
            }
            seen[board[i * n + j]] = 1;
        }
        memset(seen, 0, sizeof seen);
    }
    // check cols
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (board[j * n + i] == 0) {
                continue;
            }
            if (board[j * n + i] < 0 || board[j * n + i] > n) {
                return false;
            }
            if (seen[board[j * n + i]] == 1) {
                return false;
            }
            seen[board[j * n + i]] = 1;
        }
        memset(seen, 0, sizeof seen);
    }
    for (int i = 0; i < box; i++) {
        for (int j = 0; j < box; j++) {
            int r0 = i * box;
            int c0 = j * box;
            for (int k = 0; k < box; k++) {
                for (int l = 0; l < box; l++) {
                    int index = (k + r0) * n + c0 + l;
                    if (board[index] == 0) {
                        continue;
                    }
                    if (board[index] < 0 || board[index] > n) {
                        return false;
                    }
                    if (seen[board[index]] == 1) {
                        return false;
                    }
                    seen[board[index]] = 1;
                }
            }
            memset(seen, 0, sizeof seen);
        }
    }
    return true;
}

int main(void) {
    {
        int board[81] = {0};
        board[5] = 1;
        bool res = sudoku_valid(board, 9, 3);
        assert(res == true);
    }
    {
        int board[16] = {
            1, 3, 2, 4, 2, 4, 1, 3, 3, 1, 4, 2, 4, 2, 3, 1,
        };
        bool res = sudoku_valid(board, 4, 2);
        assert(res == true);
    }
    {
        int board[16] = {
            1, 0, 1, 4, 2, 0, 0, 3, 3, 0, 4, 2, 4, 2, 3, 1,
        };
        bool res = sudoku_valid(board, 4, 2);
        assert(res == false);
    }
    {
        int board[16] = {
            1, 0, 2, 4, 3, 4, 0, 0, 1, 0, 4, 2, 4, 2, 3, 1,
        };
        bool res = sudoku_valid(board, 4, 2);
        assert(res == false);
    }
    {
        int board[16] = {
            1, 3, 2, 4, 2, 4, 0, 3, 3, 0, 1, 2, 4, 2, 3, 1,
        };
        bool res = sudoku_valid(board, 4, 2);
        assert(res == false);
    }
    {
        int board[100] = {0};
        bool res = sudoku_valid(board, 10, 4);
        assert(res == false);
    }
    {
        int board[10002] = {0};
        bool res = sudoku_valid(board, 10002, 6);
        assert(res == false);
    }
}
