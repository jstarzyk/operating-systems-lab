#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: main path_to_file n\n");
        exit(EXIT_FAILURE);
    }

    const int n = atoi(argv[2]);

    int skey = fork();
    if (skey != 0) {
        execlp("./server", "./server", NULL);
        sleep(1);
    } else {
        for (int i = 0; i < n; i++) {
            if (fork() == 0) {
                execlp("./client", "./client", argv[1], NULL);
            }
        }
    }
}