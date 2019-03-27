#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <wait.h>

#define MAX_BUF 1024

void read_from_fifo(const char *file) {
    char buffer[MAX_BUF];
    FILE *fifo = fopen(file, "r");

    if (fifo == NULL) {
        return;
    }

    int i = 0;

    while (fgets(buffer, MAX_BUF, fifo) != NULL) {
        printf("master read: %s", buffer);
        i++;
    }

    fclose(fifo);
    usleep(500000);
    printf("%d lines read\n", i);
}

void create_fifo_if_needed(const char *file) {
    if (access(file, F_OK) == 0) {
        struct stat info;

        if (stat(file, &info) == -1) {
            fprintf(stderr, "stat error: %d\n", errno);
            exit(EXIT_FAILURE);
        }

        if (S_ISFIFO(info.st_mode) == 0) {
            fprintf(stderr, "file with given name exists and is not a FIFO special file or a pipe\n");
            exit(EXIT_FAILURE);
        }
    } else if (mkfifo(file, 0644) == -1) {
        fprintf(stderr, "mkfifo error: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}

char *_parse_args(int argc, char *argv[]) {
    if (argc == 1) {
        printf("usage: main path_to_file\n");
        exit(EXIT_SUCCESS);
    } else {
        return argv[1];
    }
}


int main(int argc, char *argv[]) {
    const char *file = _parse_args(argc, argv);
    create_fifo_if_needed(file);
    read_from_fifo(file);
}
