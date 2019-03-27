#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <wait.h>
#include <time.h>

#define PAUSE 0
#define ABORT 1
#define CONTINUE 2
#define NONE 4

int handled_action = NONE;
bool is_running = true;

void handle_sigtstp(int sig_num) {
    if (is_running) {
        printf("\nOczekuję na CTRL+Z - kontynuacja albo CTR+C - zakonczenie programu\n");
        handled_action = PAUSE;
    } else {
        printf("\n");
        handled_action = CONTINUE;
    }
}

void handle_sigint(int sig_num) {
    printf("\nOdebrano sygnał SIGINT\n");
    handled_action = ABORT;
}

int print_date() {
    int pid = fork();
    if (pid == 0) {
        execl("./print_date.sh", "./print_date.sh", NULL);
        exit(EXIT_SUCCESS);
    }
    return pid;
}

int main(int argc, char *argv[]) {
    struct sigaction action;
    action.sa_handler = handle_sigtstp;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);

    int child_pid = print_date();

    while (child_pid != 0) {
        sigaction(SIGTSTP, &action, NULL);
        signal(SIGINT, handle_sigint);
        if (is_running && handled_action == PAUSE) {
            kill(child_pid, SIGKILL);
            is_running = false;
        } else if (!is_running && handled_action == CONTINUE) {
            child_pid = print_date();
            is_running = true;
        } else if (handled_action == ABORT) {
            if (is_running) kill(child_pid, SIGKILL);
            exit(EXIT_SUCCESS);
        }
    }
}