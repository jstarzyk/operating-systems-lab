#include <unistd.h>
#include <stdlib.h>
#include <wait.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("usage: main path_to_file writers n\n");
        exit(EXIT_FAILURE);
    }

    const char master[] = "master";
//    const char master[] = "cmake-build-debug/master";
    const char slave[] = "slave";
//    const char slave[] = "cmake-build-debug/slave";
    const char *fifo = argv[1];
    const int writers = atoi(argv[2]);
    const char *n = argv[3];

    for (int i = 0; i < writers; i++) {
        int _pid = fork();

        if (_pid == 0) {
            if (execl(slave, slave, fifo, n, NULL) == -1) {
                printf("execl error: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (execl(master, master, fifo, NULL) == -1) {
        printf("execl error: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}
