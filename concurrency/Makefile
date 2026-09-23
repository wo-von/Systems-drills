# Each rung is NN-name/name.c. It builds to NN-name/name (optimized) and
# NN-name/name_tsan (ThreadSanitizer). `make` rebuilds only what changed.

CC         := gcc
CFLAGS     := -std=c11 -Wall -Wextra -Wpedantic -O2 -g -pthread
TSAN_FLAGS := -std=c11 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=thread -pthread

SRCS := $(wildcard */*.c)
BINS := $(SRCS:%.c=%)
TSAN := $(SRCS:%.c=%_tsan)

.PHONY: all clean

all: $(BINS) $(TSAN)

# Static pattern rules: only the listed targets match, so this never
# becomes a match-anything rule.
$(BINS): %: %.c
	$(CC) $(CFLAGS) -o $@ $<

$(TSAN): %_tsan: %.c
	$(CC) $(TSAN_FLAGS) -o $@ $<

clean:
	$(RM) $(BINS) $(TSAN)
