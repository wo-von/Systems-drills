#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USR 2
#define MAX_BRD 200 / 8 // larger than this is not playable in terminal, width / cell width

char symbols[MAX_USR] = {'X', 'O'};
char *names[MAX_USR] = {"player1", "player2"};

// - free OR player Symbol
struct Board {
    int n;            // board size
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
    return board->cells + board->n * row + col;
}

// for writing
static inline char *board_cell(struct Board *board, int row, int col) {
    return board->cells + board->n * row + col;
}

void del_board(struct Board *board) {
    free(board->cells);
    board->cells = NULL;
    return;
}
// return 0 in case of a problem with malloc
int set_board(struct Board *board) {

    board->counter = board->n * board->n;
    board->cells = (char *)malloc((size_t)board->counter * sizeof(char));
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

enum Result check_gamestate(const struct Board *board, const struct Move *move) {
    int x = move->x, y = move->y;
    char symbol = *(board_cell_const(board, x, y));
    int counter = board->n; // couting down
    // check row
    for (int i = 0; i < board->n; i++) {
        if (*(board_cell_const(board, x, i)) == symbol) {
            counter--;
        } else {
            break;
        }
    }
    if (counter == 0) {
        return WIN;
    }
    counter = board->n;
    // check col
    for (int i = 0; i < board->n; i++) {
        if (*(board_cell_const(board, i, y)) == symbol) {
            counter--;
        } else {
            break;
        }
    }
    if (counter == 0) {
        return WIN;
    }
    counter = board->n;
    // check diag, if on diag
    if (x == y) {
        for (int i = 0; i < board->n; i++) {
            if (*(board_cell_const(board, i, i)) == symbol) {
                counter--;
            } else {
                break;
            }
        }
    }
    if (counter == 0) {
        return WIN;
    }
    counter = board->n;
    // check anti diagonal, if on it
    if (x == board->n - 1 - y) {
        for (int i = 0; i < board->n; i++) {
            if (*(board_cell_const(board, i, board->n - 1 - i)) == symbol) {
                counter--;
            } else {
                break;
            }
        }
    }
    if (counter == 0) {
        return WIN;
    }
    if (board->counter == 0) {
        return DRAW;
    }
    return UNFINISHED;
}

void test_tictactoe(void) {
    struct Board board;
    struct Player player;
    strcpy(player.name, "player1");
    player.symbol = 'X';
    struct Move move;

    enum Result res;
    int move_res;

    // test row
    board.n = 3;
    assert(set_board(&board) == 1);
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
    board.n = 3;
    assert(set_board(&board) == 1);
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
    board.n = 3;
    assert(set_board(&board) == 1);
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
    board.n = 3;
    assert(set_board(&board) == 1);
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
    board.n = 3;
    assert(set_board(&board) == 1);
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
    board.n = 3;
    assert(set_board(&board) == 1);
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

    board.n = 3;
    assert(set_board(&board) == 1);
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
    board.n = 4;
    assert(set_board(&board) == 1);
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
    board.n = 5;
    assert(set_board(&board) == 1);
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
}
long parse_input(int argc, char *argv[]) {
    long n;
    char *endptr;
    if (argc == 1) {
        n = 3;
        return n;
    } else if (argc == 2) {
        n = strtol(argv[1], &endptr, 10);
        if (n <= 2 || (*endptr != '\0') || n > MAX_BRD) {
            fprintf(stderr, "enter a valid board size\n");
            return -1;
        }
        return n;
    } else {
        fprintf(stderr, "usage ttt [board size]\n");
        return -1;
    }
}
int main(int argc, char *argv[]) {
    struct Board board;
    long ret = parse_input(argc, argv);
    if (ret == -1) {
        return 1;
    }
    board.n = (int)ret;
    test_tictactoe();
    if (!set_board(&board)) {
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