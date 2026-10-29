#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Areas must not overlap: overlapping buffers are the caller's UB (C11 7.24.2.1).
void *my_memcpy(void *restrict dest, const void *restrict src, size_t size) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    while (size) {
        *d++ = *s++;
        size--;
    }
    return dest;
}

// Areas may overlap. dest after src: copy from the end, so every source byte is read
// before it is overwritten. Otherwise copy from the start.
void *my_memmove(void *dest, const void *src, size_t size) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    // `d > s` on pointers into different objects is UB (6.5.8p5); compare the addresses
    // as integers instead (implementation-defined, correct on flat-memory machines).
    if ((uintptr_t)d > (uintptr_t)s) {
        for (size_t i = size; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            d[i] = s[i];
        }
    }
    return dest;
}

int main(void) {
    // non-overlapping, any object type
    {
        float src[3] = {2.3f, -8.4f, 11.5f};
        float dest[3] = {0.0f, 0.0f, 0.0f};
        float *res = my_memcpy(dest, src, sizeof src);
        assert(res == dest); // returns the original dest
        for (size_t i = 0; i < sizeof src / sizeof src[0]; i++) {
            assert(dest[i] == src[i]);
        }
    }
    // size 0: nothing changes
    {
        float src[3] = {2.3f, -8.4f, 11.5f};
        float dest[3] = {0.0f, 0.0f, 0.0f};
        assert(my_memcpy(dest, src, 0) == dest);
        assert(my_memmove(dest, src, 0) == dest);
        for (size_t i = 0; i < 3; i++) {
            assert(dest[i] == 0.0f);
        }
    }
    // overlapping, dest after src: needs the backwards copy
    {
        char buf[] = "0123456789";
        assert(my_memmove(buf + 3, buf, 5) == buf + 3);
        assert(strcmp(buf, "0120123489") == 0);
    }
    // overlapping, dest before src: forwards copy
    {
        char buf[] = "0123456789";
        assert(my_memmove(buf, buf + 3, 5) == buf);
        assert(strcmp(buf, "3456756789") == 0);
    }
    // same buffer: no-op
    {
        char buf[] = "0123456789";
        my_memmove(buf, buf, 10);
        assert(strcmp(buf, "0123456789") == 0);
    }
    puts("all tests passed");
    return 0;
}
