#include <stdio.h>
#include <stdlib.h>
#include <zconf.h>
#include <signal.h>
#include <stdbool.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>

uint L, Type;

uint parent_received_signal1 = 0;
uint parent_received_signal2 = 0;
uint parent_sent_signal1 = 0;

uint child_received_signal1 = 0;
uint child_sent_signal1 = 0;
uint child_sent_signal2 = 0;

int child_pid;

struct Signals {
    int signal1;
    int signal2;
    bool with_confirmation;
};
typedef struct Signals Signals;
struct Signals signals;

int _kill(int pid, int signal) {
    int res_kill = kill(pid, signal);
    if (res_kill < 0) {
        printf("errno: %d\n", errno);
    } else if (res_kill > 0) {
        printf("kill>0 errno: %d\n", errno);
    }
    return res_kill;
}

void parent_handle_sigint(int signo) {
    printf("parent received SIGINT\n");
    _kill(child_pid, signals.signal2);
    exit(EXIT_SUCCESS);
}

void parent_handle_signals(int signo) {
    if (signo == signals.signal1) {
        parent_received_signal1++;
    } else if (signo == signals.signal2) {
        parent_received_signal2++;
    } else {
        printf("parent received unknown signal: %d\n", signo);
    }
}

void child_handle_signals(int signo) {
    if (signo == signals.signal1) {
        child_received_signal1++;
    } else {
        printf("child received unknown signal: %d\n", signo);
    }
}



void child_install_handlers() {
    struct sigaction child_action;
    sigset_t block_mask;
    sigfillset(&block_mask);
    sigdelset(&block_mask, signals.signal2);
    child_action.sa_mask = block_mask;
    child_action.sa_flags = 0;
    child_action.sa_handler = child_handle_signals;
    sigaction(signals.signal1, &child_action, NULL);
}

void parent_install_handlers() {
    struct sigaction parent_action;
    sigset_t block_mask;
    sigemptyset(&block_mask);
    parent_action.sa_mask = block_mask;
    parent_action.sa_flags = 0;
    parent_action.sa_handler = parent_handle_signals;
    sigaction(signals.signal1, &parent_action, NULL);
    sigaction(signals.signal2, &parent_action, NULL);
}


void print_parent_state() {
    printf("PARENT signal 1 (rcv: %d, sent: %d), signal 2 (rcv: %d, sent: %d)\n",
           parent_received_signal1, parent_sent_signal1, parent_received_signal2, 0
    );
}

void print_child_state() {
    printf("CHILD signal 1 (rcv: %d, sent: %d), signal 2 (rcv: %d, sent: %d)\n",
           child_received_signal1, child_sent_signal1, 0, child_sent_signal2
    );
}

int child_num_signals_to_confirm() {
    if (signals.with_confirmation) {
        return child_received_signal1 - child_sent_signal2;;
    } else {
        return 0;
    }
}


bool child_handled_all_signals_from_parent() {
    if (signals.with_confirmation) {
        return (child_received_signal1 >= L) && (child_sent_signal2 >= L);
    } else {
        return child_received_signal1 >= L;
    }
}


void run_child(int parent_pid) {
    print_child_state();

    while(!child_handled_all_signals_from_parent()) {
        for (int i = child_num_signals_to_confirm(); i > 0; i--) {
            printf("child sending confirmation (%d)\n", child_sent_signal2+1);
            if (_kill(parent_pid, signals.signal2) == 0) {
                child_sent_signal2++;
            }
        }
    }

    print_child_state();

    for (uint i = 1; i <= L; i++) {
        printf("child re-sending %s (%d) ...\n", strsignal(signals.signal1), i);
        _kill(parent_pid, signals.signal1);
        child_sent_signal1++;
    }

    print_child_state();
}


bool parent_waiting_for_confirmations() {
    if (signals.with_confirmation) {
        return parent_sent_signal1 - parent_received_signal2 > 0;
    } else {
        return false;
    }
}

bool parent_should_send_more_signals() {
    return parent_sent_signal1 < L;
}


void run_parent(int child_pid) {
    print_parent_state();

    while (parent_should_send_more_signals()) {
        printf("parent sending %s (%d) ...\n", strsignal(signals.signal1), ++parent_sent_signal1);
        _kill(child_pid, signals.signal1);

        while (parent_waiting_for_confirmations()) {
            usleep(10000);
        }
        print_parent_state();
    }

    printf("parent sent all signals (and received confirmations, if required)\n");
    print_parent_state();

    time_t timeout = time(NULL) + 10;

    while (parent_waiting_for_confirmations() && (time(NULL) < timeout)) {
        print_parent_state();
        usleep(100000);
    }

    print_parent_state();
    printf("parent sending %s ...\n", strsignal(signals.signal2));
    _kill(child_pid, signals.signal2);
}

void run() {
    switch (Type) {
        case 1:
            signals.signal1 = SIGUSR1;
            signals.signal2 = SIGUSR2;
            signals.with_confirmation = false;
            break;
        case 2:
            signals.signal1 = SIGUSR1;
            signals.signal2 = SIGUSR2;
            signals.with_confirmation = true;
            break;
        case 3:
            signals.signal1 = SIGRTMIN + 6;
            signals.signal2 = SIGRTMIN + 28;
            signals.with_confirmation = false;
            break;
        default:
            break;
    }

    if ((child_pid = fork()) == 0) {
        child_install_handlers();
        run_child(getppid());
        print_child_state();
        printf("child exiting\n");
    } else {
        signal(SIGINT, parent_handle_sigint);
        parent_install_handlers();
        usleep(100000);
        run_parent(child_pid);
        print_parent_state();
        printf("parent exiting\n");
    }
}

void print_usage() {
    printf("usage: ./main L Type\n");
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
        L = parse_unsigned_int(argv[1]);
        Type = parse_unsigned_int(argv[2]);
        if (Type < 1 || Type > 3) exit(EXIT_FAILURE);
    }

    run();

    exit(EXIT_SUCCESS);
}
