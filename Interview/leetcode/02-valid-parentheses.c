#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *starting = "({[";
char *ending = ")}]";

struct stack {
    char s[10001];
    size_t head;
    size_t tail;
};

bool check_paren(char *input) {
    struct stack stk;
    stk.head = 0;
    stk.tail = 0;
    char *p = input;
    while (*p != '\0') {
        if (strchr(starting, *p)) { // is starting
            stk.s[stk.head++] = *p++;
        } else { // is ending
            size_t ind = strchr(ending, *p) - ending;
            char pair = starting[ind];
            if (stk.head == stk.tail || stk.s[stk.head - 1] != pair) {
                return false;
            } else {
                stk.head--;
                p++;
            }
        }
    }
    return stk.head == stk.tail ? true : false;
}

int main(void) {
    {
        char test[] = "()()";
        assert(check_paren(test) == true);
    }
    {
        char test[] = "()[]{}";
        assert(check_paren(test) == true);
    }
    {
        char test[] = "([)]";
        assert(check_paren(test) == false);
    }
    {
        char test[] = "{[]}";
        assert(check_paren(test) == true);
    }
    {
        char test[] = "";
        assert(check_paren(test) == true);
    }
    {
        char test[] = "((";
        assert(check_paren(test) == false);
    }
    {
        char test[1002];
        memset(test, '(', 1001);
        test[1001] = '\0';
        assert(check_paren(test) == false);
    }
    return 0;
}
