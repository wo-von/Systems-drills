# Build any problem: make 03-number-of-islands && ./03-number-of-islands
# Build all:         make
CC     = gcc
CFLAGS = -std=c11 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined

SRCS := $(wildcard *.c)
BINS := $(SRCS:.c=)

all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(BINS)

.PHONY: all clean
