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
    int xy[2];
};
void set_board(struct Board *board) {
    for (int i = 0; i < BRD_SIZE; i++) {
        for (int j = 0; j < BRD_SIZE; j++) {
            board->state[i][j] = '-';
            (board->counter)++;
        }
    }
    return;
}

void set_players(struct Player players[], int num_player) {
    for (int i = 0; i < num_player; i++) {
        strcpy(players[i].name, names[i]);
        players[i].symbol = symbols[i];
    }
}

// should be of form int, int
// returns 0 if not
int parse_input(char *input, struct Move *move) {
    int index = 0;
    for (int i = 0; *input; i++) {
        i
    }
}

int main(int argc, char *argv[]) {
    if (argc > 2) {
        fprintf(2, "usage: tictactoe [numplayers]");
        return 1;
    }
    int num_player = 2;
    struct Board board;
    set_board(&board);
    struct Player players[MAX_USR] = {0}; // initialize to zero
    set_players(players, num_player);
    unsigned int turn = 0;
    char input[256];
    for (;;) { // player 0 always starts
        struct Move move;
        if (turn >= num_player) {
            turn %= num_player;
        }
        printf("player%d: ", turn);
        while (1) {
            if (fgets(input, sizeof(input), stdin)) {
                if (parse_input(input, &move)) {
                    break;
                } else {
                    printf("\nplayer%d: ", turn);
                }
            } else {
                printf("\nplayer%d: ", turn);
            }
        }
    }
    return 0;
}