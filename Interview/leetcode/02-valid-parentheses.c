#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char opening[] = "{[(";
static const char closing[] = "}])";

struct Stack {
    char s[10000]; // does not need to be larger than the input size
    size_t top;
};

// 1<= strlen(s) <= 10^4
bool isValid(char *s) {
    struct Stack stk = {0};

    for (char *p = s; *p; p++) {
        size_t index;
        if (strchr(opening, *p)) { // found an opening
            index = strchr(opening, *p) - opening;
            stk.s[stk.top] = *p;
            stk.top++;
        } else { // a closing
            index = strchr(closing, *p) - closing;
            if (stk.top == 0) // nothing to close
                return false;
            if (stk.s[stk.top - 1] != opening[index]) {
                return false;
            }
            stk.top--;
        }
    }
    if (stk.top != 0)
        return false;
    return true;
}

int main(void) {
    char *test = "()";
    bool res = isValid(test);
    assert(res == true);

    test = "()[]{}";
    res = isValid(test);
    assert(res == true);

    test = "(]";
    res = isValid(test);
    assert(res == false);

    test = "([])";
    res = isValid(test);
    assert(res == true);

    test = "([)]";
    res = isValid(test);
    assert(res == false);

    test = ")";
    res = isValid(test);
    assert(res == false);

    test = "())";
    res = isValid(test);
    assert(res == false);

    return 0;
}
