#include <fcntl.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 8192

void create_filter(int size, char *file) {
    srand(time(NULL));

    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) error(EXIT_FAILURE, errno, "open");

    char buffer[BUFFER_SIZE];
    size_t s = (size_t) sprintf(buffer, "%d\n", size);

    double values[size][size];
    double sum = 0.0;

    for (int i = 0; i < size; i ++) {
        for (int j = 0; j < size; j++) {
            values[i][j] = rand() / (double) RAND_MAX;
            sum += values[i][j];
        }
    }

    for (int i = 0; i < size; i ++) {
        for (int j = 0; j < size; j++) {
            values[i][j] /= sum;
        }
    }

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (s + 5 > BUFFER_SIZE) {
                if (write(fd, buffer, s) == -1) error(EXIT_FAILURE, errno, "write");
                s = 0;
            }

            s += sprintf(&buffer[s], "%lf ", values[i][j]);
        }

        buffer[s++] = '\n';
    }

    if (write(fd, buffer, s) == -1) error(EXIT_FAILURE, errno, "write");

    close(fd);
}

void print_usage_and_exit() {
    printf("usage: <program> size output\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 3) print_usage_and_exit();

    int size = atoi(argv[1]);
    char *file = argv[2];

    create_filter(size, file);

    return 0;
}