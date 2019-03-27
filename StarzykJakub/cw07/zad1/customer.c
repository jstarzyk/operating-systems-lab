#include "libbarber_shop.h"
#include <wait.h>

int sem_set;

bool have_hair_cut(struct timeval *ts, struct timeval *te, BarberShop *barber_shop, char *message, int pid) {
    if (!sem_wait(sem_set, BARBER_READY)) {
        return false;
    };

    sprintf(message, "%d - In the hairdressing chair", pid);
    print_info(ts, te, "customer", message);

    barber_shop->barber_state = CUTTING_HAIR;

    while (barber_shop->barber_state == CUTTING_HAIR) {
    }

    sprintf(message, "%d - Left", pid);
    print_info(ts, te, "customer", message);

    barber_shop->barber_state = AWAKE;

    return true;
}

void _customer(int s) {
    SObject *so_barber = barber_get(false);
    BarberShop *barber_shop = (BarberShop *) so_barber->ptr;

    SObject *so_clock = clock_get(false);
    struct timeval *ts = (struct timeval *) so_clock->ptr;

    SObject *so_fifo = fifo_get(false, barber_shop->wr_seats);
    FIFO *fifo = (FIFO *) so_fifo->ptr;

    int pid = getpid();
    char message[100];
    int i = 0;

    struct timeval te;

    while (barber_shop->is_open && i < s) {
        if (!sem_wait(sem_set, ACCESS_SEATS)) {
            break;
        }

        if (barber_shop->barber_state == SLEEPING) {
            barber_shop->barber_state = AWAKE;
            sprintf(message, "%d - Woke the barber up", pid);
            print_info(ts, &te, "customer", message);

            barber_shop->immediate_customer_pid = pid;
            sem_signal(sem_set, CUSTOMER_READY);

            sem_signal(sem_set, ACCESS_SEATS);

            if (!have_hair_cut(ts, &te, barber_shop, message, pid)) {
                break;
            };

            i++;
        } else {
            if (put_elem(fifo, pid) == 0) {
                sprintf(message, "%d - Sat in the waiting room", pid);
                print_info(ts, &te, "customer", message);
                sem_signal(sem_set, ACCESS_SEATS);

                if (!have_hair_cut(ts, &te, barber_shop, message, pid)) {
                    break;
                };

                i++;
            } else {
                sprintf(message, "%d - Left without getting a haircut", pid);
                print_info(ts, &te, "customer", message);
                sem_signal(sem_set, ACCESS_SEATS);
            }
        }


    }

    printf("%d - received %d haircut(s)\n", pid, i);

    sobject_free(so_barber, false);
    sobject_free(so_clock, false);
    sobject_free(so_fifo, false);
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: customer n s\n\tn - number of customers\n\ts - number of haircuts (for each customer)\n\n");
        exit(EXIT_FAILURE);
    }

    sem_set = semaphores_get(false);

    int n = atoi(argv[1]);
    int pid;

    for (int i = 0; i < n; i++) {
        pid = fork();
        switch (pid) {
            case -1:
                error(EXIT_FAILURE, errno, "fork");
                break;
            case 0:
                _customer(atoi(argv[2]));
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }

    while (wait(NULL) != -1) {
        usleep(10000);
    }
}
