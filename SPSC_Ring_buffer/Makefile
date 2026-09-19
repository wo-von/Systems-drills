rb: rb.c
	gcc -std=c11 -Wall -Wextra -O2 -fsanitize=address,undefined -pthread rb.c -o rb
clean:
	rm -f rb
.PHONY: clean