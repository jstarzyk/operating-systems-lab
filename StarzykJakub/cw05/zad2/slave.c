#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>

#define MAX_BUF 4096

void write_to_fifo(const char *file, const uint n) {
    int fd = open(file, O_WRONLY);
    if (fd == -1) {
        printf("open error: %d\n", errno);
    }

    int pid = getpid();
    printf("%d writing...\n", pid);

    struct timeval stime;
    gettimeofday(&stime, NULL);
    srand((unsigned int) stime.tv_usec);

    const int sleep_from = 2;
    const int sleep_to = 5;

    uint i = 0;

    while (i < n) {
        FILE *current_date = popen("date", "r");
        char buffer[128];

        if (current_date == NULL) {
            printf("popen error\n");
            exit(EXIT_FAILURE);
        }

        if (fgets(buffer, 128, current_date) == NULL) {
            printf("fgets error\n");
            exit(EXIT_FAILURE);
        }

        pclose(current_date);
        char line[MAX_BUF];
        sprintf(line, "[%d] %s", pid, buffer);

        if (write(fd, line, strlen(line)) == -1) {
            printf("write error: %d\n", errno);
            exit(EXIT_FAILURE);
        }

        if (i < n - 1) {
            sleep((unsigned int) (sleep_from + (rand() % (sleep_to - sleep_from + 1))));
        }

        i++;
    }

    close(fd);
}

uint _parse_unsigned_int(char *str) {
    int x = atoi(str);
    if (x < 0) {
        return 0;
    } else {
        return (uint) x;
    }
}

void _check_fifo(const char *file) {
    if (access(file, F_OK) == -1) {
        fprintf(stderr, "file doesn't exist\n");
        exit(EXIT_FAILURE);
    } else {
        struct stat info;

        if (stat(file, &info) == -1) {
            fprintf(stderr, "stat error: %d\n", errno);
            exit(EXIT_FAILURE);
        }

        if (S_ISFIFO(info.st_mode) == 0) {
            fprintf(stderr, "file with given name exists and is not a FIFO special file or a pipe\n");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: main path_to_file n\n");
        exit(EXIT_FAILURE);
    }

    const char *file = argv[1];
    const uint n = _parse_unsigned_int(argv[2]);

    usleep(500000);
    _check_fifo(file);

    write_to_fifo(file, n);
}
