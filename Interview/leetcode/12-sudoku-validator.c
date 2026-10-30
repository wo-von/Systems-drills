#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_N 1000

bool sudoku_valid_v0(const int *board, int n, int box) {
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

bool unit_ok(const int *vals, int n) {
    int seen[MAX_N + 1] = {0};
    for (int i = 0; i < n; i++) {
        if (vals[i] == 0) {
            continue;
        }
        if (vals[i] < 0 || vals[i] > n) {
            return false;
        }
        if (seen[vals[i]] == 1) {
            return false;
        }
        seen[vals[i]] = 1;
    }
    return true;
}

typedef void (*gatherer)(const int *board, int n, int b, int u, int *out);

typedef struct {
    gatherer g;
    int n;
} gather_kind;

void row_gatherer(const int *board, int n, int b, int u, int *out) {
    (void)b;
    for (int i = 0; i < n; i++) {
        out[i] = board[u * n + i];
    }
}

void col_gatherer(const int *board, int n, int b, int u, int *out) {
    (void)b;
    for (int i = 0; i < n; i++) {
        out[i] = board[i * n + u];
    }
}

void sqr_gatherer(const int *board, int n, int b, int u, int *out) {
    int r0 = u / b * b, c0 = (u % b) * b; // upper left corner of each sqaure
    for (int i = 0; i < n; i++) {
        int index = (r0 + i / b) * n + c0 + i % b;
        out[i] = board[index];
    }
}

void diag_gatherer(const int *board, int n, int b, int u, int *out) {
    (void)b;
    assert(u == 0 || u == 1);
    switch (u) {
    case 0:
        for (int i = 0; i < n; i++) {
            out[i] = board[i * n + i];
        }
        break;
    case 1:
        for (int i = 0; i < n; i++) {
            out[i] = board[i * n + n - 1 - i];
        }
        break;
    default:
        break;
    }
    return;
}

bool sudoku_valid(const int *board, int n, int box) {
    if (n < 0 || n > MAX_N || box <= 0 || box > n || box * box != n) {
        return false;
    }
    gather_kind kinds[] = {
        {row_gatherer, n},
        {col_gatherer, n},
        {sqr_gatherer, n},
        {diag_gatherer, 2},
    };
    int vals[MAX_N];
    for (size_t i = 0; i < (sizeof kinds / sizeof kinds[0]); i++) {
        for (int j = 0; j < kinds[i].n; j++) {
            kinds[i].g(board, n, box, j, vals);
            if (!unit_ok(vals, n)) {
                return false;
            }
        }
    }
    return true;
}

int main(void) {
    // Test boards are laid out one row per line.
    {
        int board[81] = {0};
        board[5] = 1;
        bool res = sudoku_valid_v0(board, 9, 3);
        assert(res == true);
        res = sudoku_valid(board, 9, 3);
        assert(res == true);
    }
    {
        // clang-format off
        int board[16] = {
            0, 3, 2, 4, 
            2, 0, 1, 3, 
            3, 0, 4, 2, 
            0, 2, 3, 1,
        };
        // clang-format on
        bool res = sudoku_valid_v0(board, 4, 2);
        assert(res == true);
        res = sudoku_valid(board, 4, 2);
        assert(res == true);
    }
    {
        // clang-format off
        int board[16] = {
            1, 0, 1, 4,
            2, 0, 0, 3,
            3, 0, 4, 2,
            4, 2, 3, 1,
        };
        // clang-format on
        bool res = sudoku_valid_v0(board, 4, 2);
        assert(res == false);
        res = sudoku_valid(board, 4, 2);
        assert(res == false);
    }
    {
        // clang-format off
        int board[16] = {
            1, 0, 2, 4,
            3, 4, 0, 0,
            1, 0, 4, 2,
            4, 2, 3, 1,
        };
        // clang-format on
        bool res = sudoku_valid_v0(board, 4, 2);
        assert(res == false);
        res = sudoku_valid(board, 4, 2);
        assert(res == false);
    }
    {
        // clang-format off
        int board[16] = {
            1, 3, 2, 4,
            2, 4, 0, 3,
            3, 0, 1, 2,
            4, 2, 3, 1,
        };
        // clang-format on
        bool res = sudoku_valid_v0(board, 4, 2);
        assert(res == false);
        res = sudoku_valid(board, 4, 2);
        assert(res == false);
    }
    {
        int board[100] = {0};
        bool res = sudoku_valid_v0(board, 10, 4);
        assert(res == false);
        res = sudoku_valid(board, 10, 4);
        assert(res == false);
    }
    {
        // clang-format off
        int board[16] = {
            1, 3, 2, 4, 
            2, 0, 1, 3, 
            3, 0, 4, 2, 
            0, 2, 3, 1,
        };
        // clang-format on
        bool res = sudoku_valid_v0(board, 4, 2);
        assert(res == true);
        res = sudoku_valid(board, 4, 2);
        assert(res == false);
    }
    {
        // clang-format off
        int board[16] = {
            0, 3, 2, 4, 
            2, 0, 1, 3, 
            3, 0, 4, 2, 
            4, 2, 3, 1,
        };
        // clang-format on
        bool res = sudoku_valid_v0(board, 4, 2);
        assert(res == true);
        res = sudoku_valid(board, 4, 2);
        assert(res == false);
    }
    {
        int board[10002] = {0};
        bool res = sudoku_valid_v0(board, 10002, 6);
        assert(res == false);
        res = sudoku_valid(board, 10002, 6);
        assert(res == false);
    }
}
