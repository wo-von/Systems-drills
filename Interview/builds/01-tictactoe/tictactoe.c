#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USR 2
#define MAX_BRD (200 / 8) // larger than this is not playable in terminal, width / cell width

char symbols[MAX_USR] = {'X', 'O'};
char *names[MAX_USR] = {"player1", "player2"};

// - free OR player Symbol
struct Board {
    int n;            // board size
    int k;            // win requirement
    long int counter; // how many free left
    char *cells;
};

struct Player {
    char name[128];
    char symbol;
};

struct Move {
    int x;
    int y;
};

enum Result {
    UNFINISHED = 0,
    WIN,
    DRAW,
};

// for reading
static inline const char *board_cell_const(const struct Board *board, int row, int col) {
    assert(row >= 0 && row < board->n && col >= 0 && col < board->n);
    return board->cells + board->n * row + col;
}

// for writing
static inline char *board_cell(struct Board *board, int row, int col) {
    assert(row >= 0 && row < board->n && col >= 0 && col < board->n);
    return board->cells + board->n * row + col;
}

void del_board(struct Board *board) {
    free(board->cells);
    board->cells = NULL;
    return;
}
// return 0 if n or k is out of range, or in case of a problem with malloc
int set_board(struct Board *board, int n, int k) {
    board->cells = NULL;
    if (n < 3 || n > MAX_BRD || k < 3 || k > n) {
        fprintf(stderr, "invalid board: n=%d k=%d\n", n, k);
        return 0;
    }
    board->n = n;
    board->k = k;
    board->counter = n * n;
    board->cells = malloc((size_t)board->counter);
    if (board->cells == NULL) {
        fprintf(stderr, "malloc failed\n");
        return 0;
    }
    for (int i = 0; i < board->n; i++) {
        for (int j = 0; j < board->n; j++) {
            *(board_cell(board, i, j)) = '-';
        }
    }
    return 1;
}

void set_players(struct Player players[], int num_player) {
    for (int i = 0; i < num_player; i++) {
        strcpy(players[i].name, names[i]);
        players[i].symbol = symbols[i];
    }
}

void print_board(const struct Board *board) {
    printf("\n");
    for (int i = 0; i < board->n; i++) {
        for (int j = 0; j < board->n; j++) {
            printf("%c\t", *(board_cell_const(board, i, j)));
        }
        printf("\n");
    }
    return;
}
// returns 1 if the move was possible, 0 otherwise
int make_move(struct Move *move, struct Board *board, const char symbol) {
    int x = move->x, y = move->y;
    if (x < 0 || x >= board->n || y < 0 || y >= board->n) {
        printf("move out of bounds\n");
        return 0;
    }

    if (*(board_cell_const(board, x, y)) == '-') {
        *(board_cell(board, x, y)) = symbol;
        (board->counter)--;
        return 1;
    } else {
        printf("Square taken\n");
        return 0;
    }
}
// count the marks equal to the one at move, walking from the cell next to move in
// direction (dr, dc) until a different mark or the edge; the move itself is not counted
int count_dir(const struct Board *board, const struct Move *move, int dr, int dc) {
    char symbol = *(board_cell_const(board, move->x, move->y));
    int x = move->x + dr, y = move->y + dc;
    int count = 0;
    while (x >= 0 && x < board->n && y >= 0 && y < board->n &&
           (*(board_cell_const(board, x, y)) == symbol)) {
        count++;
        x += dr;
        y += dc;
    }
    return count;
}

// returns 1 if the move completes k or more in a row in any direction, 0 otherwise
int is_win(const struct Board *board, const struct Move *move) {
    // row, col, diag, anti diag; the opposite direction is the negation
    const int dirs[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    for (int d = 0; d < 4; d++) {
        int dr = dirs[d][0], dc = dirs[d][1];
        int run = 1 + count_dir(board, move, dr, dc) + count_dir(board, move, -dr, -dc);
        if (run >= board->k) {
            return 1;
        }
    }
    return 0;
}

enum Result check_gamestate(const struct Board *board, const struct Move *move) {
    if (is_win(board, move)) {
        return WIN;
    }
    if (board->counter == 0) {
        return DRAW;
    }
    return UNFINISHED;
}

// test helper: place a mark, fail loudly if the square was not free, return the move
static struct Move place(struct Board *board, int row, int col, char symbol) {
    struct Move move = {row, col};
    int ok = make_move(&move, board, symbol);
    assert(ok == 1);
    return move;
}

void test_tictactoe(void) {
    struct Board board;
    struct Player player;
    strcpy(player.name, "player1");
    player.symbol = 'X';
    struct Move move;

    enum Result res;
    int move_res;
    int ok;

    // test row
    ok = set_board(&board, 3, 3);
    assert(ok == 1);
    move.x = 0, move.y = 0;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    move.x = 0, move.y = 1;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    move.x = 0, move.y = 2;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == WIN);
    del_board(&board);

    // test col
    ok = set_board(&board, 3, 3);
    assert(ok == 1);
    move.x = 0, move.y = 0;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    move.x = 1, move.y = 0;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    move.x = 2, move.y = 0;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == WIN);
    del_board(&board);

    // test diag
    ok = set_board(&board, 3, 3);
    assert(ok == 1);
    move.x = 0, move.y = 0;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    move.x = 1, move.y = 1;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    move.x = 2, move.y = 2;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == WIN);
    del_board(&board);

    // test antidiag
    ok = set_board(&board, 3, 3);
    assert(ok == 1);
    move.x = 0, move.y = 2;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    move.x = 1, move.y = 1;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    move.x = 2, move.y = 0;
    move_res = make_move(&move, &board, player.symbol);
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == WIN);
    del_board(&board);

    // test draw
    ok = set_board(&board, 3, 3);
    assert(ok == 1);
    move.x = 0, move.y = 1;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 0, move.y = 2;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 1, move.y = 0;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 1, move.y = 1;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 1, move.y = 2;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 2, move.y = 0;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 2, move.y = 1;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 2, move.y = 2;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 0, move.y = 0;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == DRAW);
    del_board(&board);

    // test UNFINISHED
    ok = set_board(&board, 3, 3);
    assert(ok == 1);
    move.x = 0, move.y = 0;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 0, move.y = 2;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 1, move.y = 1;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == UNFINISHED);
    del_board(&board);

    ok = set_board(&board, 3, 3);
    assert(ok == 1);
    move.x = 0, move.y = 1;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 0, move.y = 2;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 0, move.y = 0;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == UNFINISHED);
    del_board(&board);

    // test 4x4 near miss
    ok = set_board(&board, 4, 4);
    assert(ok == 1);
    move.x = 0, move.y = 0;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 0, move.y = 1;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 1, move.y = 1;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 0, move.y = 2;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 2, move.y = 1;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 3, move.y = 3;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 2, move.y = 2;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == UNFINISHED);
    del_board(&board);

    // test 5x5 board
    ok = set_board(&board, 5, 5);
    assert(ok == 1);
    move.x = 0, move.y = 0;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 0, move.y = 1;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 1, move.y = 1;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 0, move.y = 2;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 2, move.y = 2;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 0, move.y = 3;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 3, move.y = 3;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    move.x = 0, move.y = 4;
    move_res = make_move(&move, &board, 'O');
    assert(move_res == 1);
    move.x = 4, move.y = 4;
    move_res = make_move(&move, &board, 'X');
    assert(move_res == 1);
    res = check_gamestate(&board, &move);
    assert(res == WIN);
    del_board(&board);

    // k in a row: short diagonal off the main ones, 5x5, k = 3
    ok = set_board(&board, 5, 3);
    assert(ok == 1);
    place(&board, 1, 0, 'X');
    place(&board, 2, 1, 'X');
    move = place(&board, 3, 2, 'X');
    res = check_gamestate(&board, &move);
    assert(res == WIN);
    del_board(&board);

    // two marks are not three: the move must be counted once
    ok = set_board(&board, 5, 3);
    assert(ok == 1);
    place(&board, 0, 0, 'X');
    move = place(&board, 0, 1, 'X');
    res = check_gamestate(&board, &move);
    assert(res == UNFINISHED);
    del_board(&board);

    // X X - X X is not 4 in a row
    ok = set_board(&board, 5, 4);
    assert(ok == 1);
    place(&board, 2, 0, 'X');
    place(&board, 2, 1, 'X');
    place(&board, 2, 3, 'X');
    move = place(&board, 2, 4, 'X');
    res = check_gamestate(&board, &move);
    assert(res == UNFINISHED);
    // filling the gap makes 5, more than k, still a win
    move = place(&board, 2, 2, 'X');
    res = check_gamestate(&board, &move);
    assert(res == WIN);
    del_board(&board);

    // the walk must stop at the edge, not wrap from (1, 0) to (0, 4)
    ok = set_board(&board, 5, 3);
    assert(ok == 1);
    place(&board, 0, 4, 'X');
    place(&board, 1, 1, 'X');
    move = place(&board, 1, 0, 'X');
    res = check_gamestate(&board, &move);
    assert(res == UNFINISHED);
    del_board(&board);

    // a board that can't be won, or a k below 3, is never created
    ok = set_board(&board, 3, 4);
    assert(ok == 0);
    ok = set_board(&board, 5, 2);
    assert(ok == 0);
}
// parse s as a whole decimal number in [lo, hi]; returns 1 on success
// the range check happens on the long, before narrowing to int
int parse_int(const char *s, long lo, long hi, int *out) {
    char *endptr;
    long v = strtol(s, &endptr, 10);
    if (endptr == s || *endptr != '\0' || v < lo || v > hi) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

// ttt [boardsize [winsize]]: n defaults to 3, k defaults to n
int parse_input(int argc, char *argv[], int *n, int *k) {
    if (argc > 3) {
        fprintf(stderr, "usage ttt [boardsize [winsize]]\n");
        return -1;
    }
    *n = 3;
    if (argc >= 2 && !parse_int(argv[1], 3, MAX_BRD, n)) {
        fprintf(stderr, "enter a valid board size (3..%d)\n", MAX_BRD);
        return -1;
    }
    *k = *n;
    if (argc == 3 && !parse_int(argv[2], 3, *n, k)) {
        fprintf(stderr, "enter a valid win requirement (3..%d)\n", *n);
        return -1;
    }
    return 1;
}
int main(int argc, char *argv[]) {
    struct Board board;
    int n, k;
    if (parse_input(argc, argv, &n, &k) == -1) {
        return 1;
    }
    test_tictactoe();
    if (!set_board(&board, n, k)) {
        return 1;
    }
    unsigned int num_player = 2;
    struct Player players[MAX_USR] = {0}; // initialize to zero
    set_players(players, num_player);
    unsigned int turn = 0;
    char input[256];
    for (;;) { // player 0 always starts
        struct Move move;
        print_board(&board);
        if (turn >= num_player) {
            turn %= num_player;
        }
        struct Player *curr_player = &players[turn];
        while (1) {
            printf("%s: ", curr_player->name);
            if (fgets(input, sizeof(input), stdin)) {
                if (sscanf(input, "%d , %d", &move.x, &move.y) == 2) {
                    break;
                } else {
                    printf("\n");
                    continue;
                }
            } else {
                fprintf(stderr, "ttt: game aborted\n");
                del_board(&board);
                return 1;
            }
        }
        int res = 0;
        if (make_move(&move, &board, curr_player->symbol)) {
            res = check_gamestate(&board, &move);
        } else {
            continue;
        }
        switch (res) {
        case WIN:
            printf("%s wins\n", curr_player->name);
            print_board(&board);
            del_board(&board);
            return 0;
            break;
        case DRAW:
            printf("it is a draw!\n");
            del_board(&board);
            return 0;
            break;
        case UNFINISHED:
            turn++;
            continue;
            break;
        default:
            continue;
        }
    }
    return 0;
}