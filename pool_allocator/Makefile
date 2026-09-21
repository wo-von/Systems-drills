CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -fsanitize=address,undefined

pa: pa.c
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -rf pa
all: pa
.PHONY: clean all