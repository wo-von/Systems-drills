#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void die(char *why) {
    perror(why);
    exit(1);
}
// reads one byte, returns zero on EOF
int read_byte(int fd, char *buf) {
    ssize_t n;
    do {
        n = read(fd, buf, 1);
    } while (n < 0 && errno == EINTR);
    if (n < 0) {
        die("read byte");
    }
    return n == 1;
}
// writes one byte, returns zero if cannot
int write_byte(int fd, char *buf) {
    ssize_t n;
    do {
        n = write(fd, buf, 1);
    } while (n < 0 && errno == EINTR);
    if (n != 1) {
        die("write_byte");
    }
    return n == 1;
}
double timeit() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    long reps = 100000;
    if (argc > 2) {
        fprintf(stdout, "usage: pingpong repnumber\n");
        return 1;
    }
    if (argc == 2) {
        reps = atol(argv[1]);
    }
    int p2c[2];
    int c2p[2];
    if (pipe(p2c) < 0)
        die("pipe p2c");
    if (pipe(c2p) < 0)
        die("pipe c2p");

    pid_t pid = fork();

    if (pid < 0) {
        die("fork failed");
    }
    char ball = 'x';
    if (pid == 0) { // in child
        // reads from parent, writes back
        close(p2c[1]);
        close(c2p[0]);
        while (read_byte(p2c[0], &ball)) {
            write_byte(c2p[1], &ball);
        }
        close(p2c[0]);
        close(c2p[1]);
        _exit(0);
    }
    double then = timeit();

    // parents read from child and writes back
    close(c2p[1]);
    close(p2c[0]);
    for (long i = 0; i < reps; i++) {
        write_byte(p2c[1], &ball);
        if (!read_byte(c2p[0], &ball)) {
            die("unexpected EOF from child");
        }
    }
    double now = timeit();

    close(c2p[0]);
    close(p2c[1]);
    int status;
    waitpid(pid, &status, 0);
    double elapsed = now - then;
    double rate = reps / elapsed;

    printf("exchanges   %ld\n", reps);
    printf("rate   %f\n", rate);
    printf("elapsed   %f\n", elapsed);
    printf("usec / exchange   %f\n", elapsed * 1e6 / reps);
    return 0;
}