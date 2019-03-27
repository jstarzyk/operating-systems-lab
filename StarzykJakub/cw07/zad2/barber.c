#include "libbarber_shop.h"

FIFO *fifo;
BarberShop *barber_shop;
struct timeval *ts;
sem_t **sem_set;

void handle_sig(int signo) {
    barber_shop->is_open = false;
}

void _barber() {
    char message[100];
    int pid;

    struct timeval te;

    while (barber_shop->is_open) {
        if (is_empty(fifo)) {
            barber_shop->barber_state = SLEEPING;
            print_info(ts, &te, "barber", "Sleeping...");

            if (!_sem_wait(sem_set, CUSTOMER_READY)) {
                break;
            }
            print_info(ts, &te, "barber", "Awake");

            pid = barber_shop->immediate_customer_pid;
        } else {
            if (!_sem_wait(sem_set, ACCESS_SEATS)) {
                break;
            }
            pid = get_elem(fifo);
            sprintf(message, "Invited a customer to sit in the chair (customer %d)", pid);
            print_info(ts, &te, "barber", message);
            _sem_signal(sem_set, ACCESS_SEATS);
        }

        _sem_signal(sem_set, BARBER_READY);

        while (barber_shop->barber_state == AWAKE) {}

        sprintf(message, "Cutting hair (customer %d)...", pid);
        print_info(ts, &te, "barber", message);
        sprintf(message, "Done (customer %d)", pid);
        print_info(ts, &te, "barber", message);
        barber_shop->barber_state = DONE;

        while (barber_shop->barber_state == DONE) {}
    }

    printf("\n");
    print_info(ts, &te, "barber", "Left");
}

void print_usage_and_exit() {
    printf("usage: barber n\n\tn - number of seats in the waiting room (0 <= n <= %d)\n\n", MAX_WR_SEATS);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int n;

    if (argc != 2) {
        print_usage_and_exit();
    } else {
        n = atoi(argv[1]);
        if (n > MAX_WR_SEATS || n < 0) {
            print_usage_and_exit();
        }
    }

    sem_set = semaphores_get(true);

    SObject *so_barber = barber_get(true);
    barber_shop = (BarberShop *) so_barber->ptr;
    barber_shop->wr_seats = n;

    SObject *so_clock = clock_get(true);
    ts = (struct timeval *) so_clock->ptr;

    SObject *so_fifo = fifo_get(true, barber_shop->wr_seats);
    fifo = (FIFO *) so_fifo->ptr;

    struct sigaction action;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    action.sa_handler = handle_sig;
    sigaction(SIGINT, &action, NULL);

    _barber();

    sobject_free(so_barber, true);
    sobject_free(so_clock, true);
    sobject_free(so_fifo, true);

    for (int i = 0; i < 3; i++) {
        if (sem_close(sem_set[i]) == -1) {
            error(EXIT_FAILURE, errno, "sem_close");
        }
    }
}
