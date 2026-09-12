/*
 * ping-pong: bounce a single byte between a parent and child process
 * over a pair of pipes (one per direction), and measure how many
 * round-trip exchanges per second the pair can sustain.
 *
 * Usage: ./pingpong [num_exchanges]
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

/* read exactly one byte, retrying on EINTR; returns 0 on EOF, 1 on success */
static int read_byte(int fd, char *buf) {
    ssize_t n;
    do {
        n = read(fd, buf, 1);
    } while (n < 0 && errno == EINTR);
    if (n < 0) die("read");
    return n == 1;
}

static void write_byte(int fd, char b) {
    ssize_t n;
    do {
        n = write(fd, &b, 1);
    } while (n < 0 && errno == EINTR);
    if (n != 1) die("write");
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    long exchanges = 100000;
    if (argc > 1) {
        exchanges = atol(argv[1]);
        if (exchanges <= 0) {
            fprintf(stderr, "usage: %s [num_exchanges]\n", argv[0]);
            return 1;
        }
    }

    int p2c[2]; /* parent -> child */
    int c2p[2]; /* child -> parent */
    if (pipe(p2c) < 0) die("pipe p2c");
    if (pipe(c2p) < 0) die("pipe c2p");

    pid_t pid = fork();
    if (pid < 0) die("fork");

    if (pid == 0) {
        /* child: reads from p2c, writes to c2p */
        close(p2c[1]);
        close(c2p[0]);

        char byte;
        while (read_byte(p2c[0], &byte)) {
            write_byte(c2p[1], byte);
        }

        close(p2c[0]);
        close(c2p[1]);
        _exit(0);
    }

    /* parent: writes to p2c, reads from c2p */
    close(p2c[0]);
    close(c2p[1]);

    char byte = 'x';
    double start = now_seconds();

    for (long i = 0; i < exchanges; i++) {
        write_byte(p2c[1], byte);
        if (!read_byte(c2p[0], &byte)) {
            fprintf(stderr, "unexpected EOF from child\n");
            break;
        }
    }

    double elapsed = now_seconds() - start;

    /* closing p2c[1] sends EOF to child, ending its loop */
    close(p2c[1]);
    close(c2p[0]);

    int status;
    waitpid(pid, &status, 0);

    double rate = exchanges / elapsed;
    printf("exchanges:        %ld\n", exchanges);
    printf("elapsed:          %.6f s\n", elapsed);
    printf("exchanges/sec:    %.2f\n", rate);
    printf("usec/exchange:    %.4f\n", elapsed * 1e6 / exchanges);

    return 0;
}
