#include <stdio.h>
#include <stdlib.h>
#include <zconf.h>
#include <signal.h>
#include <stdbool.h>
#include <bits/types/siginfo_t.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>


struct ProcData {
    int pid;
    int return_signal;
    int sleep_time;
};
typedef struct ProcData ProcData;

uint n = 0, k = 0;
bool allow_run_flag = false;
bool received_sigcont = false;
uint continue_requests = 0;
uint returns = 0;

uint forked = 0;
int *requested_pids;
int *all_pids;
ProcData *proc_data;


void print_pending_signals(const char * label) {
    sigset_t pending;
    if (sigpending(&pending) == 0) {
        for (int i = 1; i <= SIGRTMAX; i++) {
            if (sigismember(&pending, i) == 1) {
                printf("%s: pending signal %s\n", label, strsignal(i));
            }
        };
    } else {
        printf("sigpending error\n");
    }
}

void handle_sigint(int signo) {
    for (uint i = 0; i < n; i++) {
        if (waitpid(all_pids[i], NULL, 0) < 0 && errno == ECHILD) {
            continue;
        } else {
            kill(all_pids[i], SIGTERM);
        }
    }
    exit(EXIT_FAILURE);
}

void handle_sigrt(int signo, siginfo_t *info, void *context) {
    ProcData data;
    data.pid = info->si_value.sival_int;
    data.return_signal = signo;
    printf("parent handled SIGRTMIN+%d from %d\n", data.return_signal - SIGRTMIN, data.pid);
    proc_data[returns] = data;
    returns++;
}


void handle_sigusr1(int signo, siginfo_t *info, void *context) {
    int child_pid = info->si_value.sival_int;
    printf("%d sent SIGUSR1\n", child_pid);
    requested_pids[continue_requests++] = child_pid;
    if (continue_requests == k) {
        allow_run_flag = true;
    }
}

void handle_sigcont(int signo) {
    received_sigcont = true;
}


int run_child(int s) {
    srand((unsigned int) time(NULL) + s * s);
    int sleep_time = rand() % 11;
    printf("%d sleeping for %d seconds\n", getpid(), sleep_time);
    sleep((uint) sleep_time);

    union sigval u;
    u.sival_int = getpid();
    int ppid = getppid();

    signal(SIGCONT, handle_sigcont);

    int res = sigqueue(ppid, SIGUSR1, u);
    printf("%d: sigqueue USR1 result = %d\n", getpid(), res);

    while(true) {
        if (received_sigcont) {
            printf("%d handled SIGCONT\n", getpid());
            int return_signal = SIGRTMIN + (rand() % (SIGRTMAX - SIGRTMIN + 1));

            union sigval v;
            v.sival_int = getpid();
            sigqueue(ppid, return_signal, v);
//            printf("%d: sigqueue (SIGRTMIN+%d) result = %d\n", getpid(), return_signal - SIGRTMIN, res_sigqueue);
            exit(sleep_time);
        } else {
            usleep(100000);
        }
    }
}

void spawn(int n) {
    int child_pid;
    usleep((__useconds_t) (rand() % 50000));
    for (int i = 0; i < n; i++) {
        if ((child_pid = fork()) == 0) {
            run_child(i);
            return;
        } else {
            srand((unsigned int) time(NULL));
            all_pids[forked++] = child_pid;
        }
    }
}

void send_sigcont(pid_t pid) {
    union sigval u;
    u.sival_int = 0;
    int res = sigqueue(pid, SIGCONT, u);
    if (res < 0) {
        printf("parent: sigqueue (SIGCONT) failed\n");
    }
}

void allow_run() {
    struct sigaction action1;
    action1.sa_flags = SA_SIGINFO;
    action1.sa_sigaction = handle_sigrt;
    sigemptyset(&action1.sa_mask);

    for (int j = SIGRTMIN; j <= SIGRTMAX; j++) {
        sigaction(j, &action1, NULL);
    }

    for (uint i = 0; i < k; i++) {
        send_sigcont(requested_pids[i]);
    }
}


void parent_wait_for_children_terminate() {
    while (true) {
        int status = 0;
        int wpid = waitpid(0, &status, 0);
        if (wpid > 0) {
            int exit_code = status / 256;
            printf("%d exited with exit code (sleep time) %d, status=%d\n", wpid, exit_code, status);
            for (int j = 0; j < n; j++) {
                if (proc_data[j].pid == wpid) {
                    proc_data[j].sleep_time = status;
                    break;
                }
            }
        } else if ((wpid < 0) && (errno == ECHILD)) {
            return;
        }
    }
}

void start_scheduler(uint n, uint k) {
    struct sigaction action;
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handle_sigusr1;
    sigset_t block_mask;
    sigemptyset(&block_mask);

    for (int j = SIGRTMIN; j <= SIGRTMAX; j++) {
        sigaddset(&block_mask, j);
    }

    action.sa_mask = block_mask;
    sigaction(SIGUSR1, &action, NULL);


    spawn(n);

    signal(SIGINT, handle_sigint);

    while (!allow_run_flag) {
        usleep(10000);
    }
    allow_run();

    int r = k;
    while (r < n) {
        if (requested_pids[r] != -1) {
            send_sigcont(requested_pids[r]);
            r++;
        }
    }

    parent_wait_for_children_terminate();
}

void print_usage() {
    printf("usage: ./main n k\n");
    exit(EXIT_FAILURE);
}

uint parse_unsigned_int(char *str) {
    int x = atoi(str);
    if (x < 0) {
        return 0;
    } else {
        return (uint) x;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage();
    } else {
        n = parse_unsigned_int(argv[1]);
        k = parse_unsigned_int(argv[2]);
        if (k > n) k = n;
    }

    all_pids = malloc(n * sizeof(int));
    proc_data = malloc(n * sizeof(ProcData));
    for (uint i = 0; i < n; i++) {
        proc_data[i].pid = proc_data[i].return_signal = proc_data[i].sleep_time = -1;
    }
    requested_pids = malloc(n * sizeof(int));
    for (uint i = 0; i < n; i++) {
        requested_pids[i] = -1;
    }
    start_scheduler(n, k);

    return 0;
}