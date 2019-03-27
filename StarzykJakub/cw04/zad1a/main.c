#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <wait.h>
#include <time.h>

bool executing_loop = true;

void handle_sigtstp(int sig_num) {
    if (executing_loop) {
        printf("\nOczekuję na CTRL+Z - kontynuacja albo CTR+C - zakonczenie programu\n");
        executing_loop = false;
    } else {
        printf("\n");
        executing_loop = true;
    }
}

void handle_sigint(int sig_num) {
    printf("\nOdebrano sygnał SIGINT\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    struct sigaction action;
    action.sa_handler = handle_sigtstp;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);

    time_t current_time;

    while (true) {
        sigaction(SIGTSTP, &action, NULL);
        signal(SIGINT, handle_sigint);

        if (executing_loop) {
            char buffer[30];
            current_time = time(NULL);
            strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&current_time));
            printf("%s\n", buffer);
        }

        sleep(1);
    }
}
