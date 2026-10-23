#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *opening = "({[";
const char *closing = ")}]";

struct Stack {
    char buff[20000];
    size_t top;
};

bool stack_empty(struct Stack *st) {
    return st->top == 0;
}

bool isValid(char *s) {
    size_t str_len = strlen(s);
    if (str_len % 2 == 1) {
        return false;
    }
    struct Stack stk = {0};
    for (size_t i = 0; i < str_len; i++) {
        char *match = strchr(opening, s[i]);
        if (match) { // is a opening
            stk.buff[stk.top] = s[i];
            stk.top++;
        } else {
            match = strchr(closing, s[i]);
            int index = match - closing;
            if (!stk.top) { // we have a closing and stack is empty
                return false;
            }
            if (stk.buff[stk.top - 1] != opening[index]) {
                return false;
            }
            stk.top--;
        }
    }
    if (stk.top != 0) {
        return false;
    } else {
        return true;
    }
}

int main(void) {
    {
        char *test = "(){}[]";
        bool res = isValid(test);
        assert(res == true);
    }
    {
        char *test = "(]";
        bool res = isValid(test);
        assert(res == false);
    }
    {
        char *test = "([)]";
        bool res = isValid(test);
        assert(res == false);
    }
    {
        char *test = "{[]}";
        bool res = isValid(test);
        assert(res == true);
    }
    {
        {
            char *test = "))";
            bool res = isValid(test);
            assert(res == false);
        }
        char *test = "((";
        bool res = isValid(test);
        assert(res == false);
    }
    return 0;
}
