# Build any problem: make 03-number-of-islands && ./03-number-of-islands
# Build all:         make
CC     = gcc
CFLAGS = -std=c11 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined
CXX      = g++
CXXFLAGS = -std=c++17 -g -Wall -Wextra -Wpedantic -D_GLIBCXX_ASSERTIONS -fsanitize=address,undefined

SRCS := $(wildcard *.c)
CPPSRCS := $(wildcard *.cpp)
BINS := $(SRCS:.c=) $(CPPSRCS:.cpp=)

all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(BINS)

.PHONY: all clean
