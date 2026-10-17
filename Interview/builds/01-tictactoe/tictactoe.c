#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BRD_SIZE 3
#define MAX_USR 2

char symbols[MAX_USR] = {'X', 'O'};
char *names[MAX_USR] = {"player1", "player2"};

// - free OR player Symbol
struct Board {
    char state[BRD_SIZE][BRD_SIZE];
    int counter; // how many free left
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

void set_board(struct Board *board) {
    for (int i = 0; i < BRD_SIZE; i++) {
        for (int j = 0; j < BRD_SIZE; j++) {
            board->state[i][j] = '-';
        }
    }
    board->counter = BRD_SIZE * BRD_SIZE;
    return;
}

void set_players(struct Player players[], int num_player) {
    for (int i = 0; i < num_player; i++) {
        strcpy(players[i].name, names[i]);
        players[i].symbol = symbols[i];
    }
}

void print_board(struct Board *board) {
    printf("\n");
    for (int i = 0; i < BRD_SIZE; i++) {
        for (int j = 0; j < BRD_SIZE; j++) {
            printf("%c\t", board->state[i][j]);
        }
        printf("\n");
    }
    return;
}
// returns 1 if the move was possible, 0 otherwise
int make_move(struct Move *move, struct Board *board, struct Player *curr_player) {
    int x = move->x, y = move->y;
    if (x < 0 || x >= BRD_SIZE || y < 0 || y >= BRD_SIZE) {
        printf("move out of bounds\n");
        return 0;
    }

    if (board->state[x][y] == '-') {
        board->state[x][y] = curr_player->symbol;
        (board->counter)--;
        return 1;
    } else {
        printf("Square taken\n");
        return 0;
    }
}

enum Result check_gamestate(struct Board *board, struct Player *curr_player, struct Move *move) {
    int x = move->x, y = move->y;
    int counter = BRD_SIZE; // couting down
    // check row
    for (int i = 0; i < BRD_SIZE; i++) {
        if (board->state[x][i] == curr_player->symbol) {
            counter--;
        } else {
            break;
        }
    }
    if (counter == 0) {
        return WIN;
    }
    counter = BRD_SIZE;
    // check col
    for (int i = 0; i < BRD_SIZE; i++) {
        if (board->state[i][y] == curr_player->symbol) {
            counter--;
        } else {
            break;
        }
    }
    if (counter == 0) {
        return WIN;
    }
    counter = BRD_SIZE;
    // check diag, if on diag
    if (x == y) {
        for (int i = 0; i < BRD_SIZE; i++) {
            if (board->state[i][i] == curr_player->symbol) {
                counter--;
            } else {
                break;
            }
        }
    }
    if (counter == 0) {
        return WIN;
    }
    counter = BRD_SIZE;
    // check anti diagonal, if on it
    if (x == BRD_SIZE - 1 - y) {
        for (int i = 0; i < BRD_SIZE; i++) {
            if (board->state[i][BRD_SIZE - 1 - i] == curr_player->symbol) {
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

int main(void) {
    unsigned int num_player = 2;
    struct Board board;
    set_board(&board);
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
                return 1;
            }
        }
        int res = 0;
        if (make_move(&move, &board, curr_player)) {
            res = check_gamestate(&board, curr_player, &move);
        } else {
            continue;
        }
        switch (res) {
        case WIN:
            printf("%s wins\n", curr_player->name);
            print_board(&board);
            return 0;
            break;
        case DRAW:
            printf("it is a draw!\n");
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