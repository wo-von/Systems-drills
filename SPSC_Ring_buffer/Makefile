CFLAGS = -std=c11 -Wall -Wextra -O2 -fsanitize=address,undefined -pthread

all: rb rb_lock
rb: rb.c
	gcc $(CFLAGS) rb.c -o rb
rb_lock: rb_lock.c
	gcc $(CFLAGS) rb_lock.c -o rb_lock
clean:
	rm -f rb rb_lock
.PHONY: all clean
