#include "libbarber_shop.h"

key_t key_get(int proj_id) {
    const char *home = getenv("HOME");
    key_t key = ftok(home, proj_id);

    if (key == -1) {
        error(EXIT_FAILURE, errno, "ftok");
    }

    return key;
}

int semaphores_get(bool new) {
    int flags = SO_MODE;
    if (new) flags |= IPC_CREAT;

    int sem_set = semget(key_get(SEMAPHORES_KEY), 3, flags);
    if (sem_set == -1) {
        error(EXIT_FAILURE, errno, "semget");
    }

    if (new) {
        union semun s = {0};
        if (semctl(sem_set, BARBER_READY, SETVAL, s) == -1) {
            error(EXIT_FAILURE, errno, "semctl");
        }
        if (semctl(sem_set, CUSTOMER_READY, SETVAL, s) == -1) {
            error(EXIT_FAILURE, errno, "semctl");
        }
        s.val = 1;
        if (semctl(sem_set, ACCESS_SEATS, SETVAL, s) == -1) {
            error(EXIT_FAILURE, errno, "semctl");
        }
    }

    return sem_set;
}

SObject *sobject_new(key_t key, size_t size, int flags) {
    int id = shmget(key, size, flags);
    if (id == -1) {
        error(EXIT_FAILURE, errno, "shmget");
    }

    void *ptr = shmat(id, NULL, 0);
    if (ptr == (void *) -1) {
        error(EXIT_FAILURE, errno, "shmat");
    }

    SObject *result = malloc(sizeof(SObject));
    result->id = id;
    result->ptr = ptr;

    return result;
}

SObject *barber_get(bool new) {
    int flags = SO_MODE;
    if (new) flags |= IPC_CREAT;

    SObject *result = sobject_new(key_get(BARBER_KEY), sizeof(BarberShop), flags);

    if (new) {
        BarberShop *barber_shop = (BarberShop *) result->ptr;
        barber_shop->is_open = true;
        barber_shop->barber_state = AWAKE;
    }

    return result;
}

SObject *clock_get(bool new) {
    int flags = SO_MODE;
    if (new) flags |= IPC_CREAT;

    SObject *result = sobject_new(key_get(CLOCK_KEY), sizeof(struct timeval), flags);

    if (new) {
        if (gettimeofday((struct timeval *) result->ptr, NULL) == -1) {
            error(EXIT_FAILURE, errno, "gettimeofday");
        }
    }

    return result;
}

SObject *fifo_get(bool new, int size) {
    int flags = SO_MODE;
    if (new) flags |= IPC_CREAT;

    SObject *result = sobject_new(key_get(FIFO_KEY), sizeof(FIFO), flags);

    if (new) {
        FIFO *fifo = (FIFO *) result->ptr;
        fifo->size = size + 1;
        fifo->head = fifo->tail = 0;
        for (int i = 0; i < fifo->size; i++) {
            fifo->data[i] = -1;
        }
    }

    return result;
}


bool is_empty(FIFO *fifo) {
    return fifo->head == fifo->tail;
}

bool is_full(FIFO *fifo) {
    return fifo->head == (fifo->tail + 1) % fifo->size;
}


int put_elem(FIFO *fifo, int new) {
    if (is_full(fifo)) {
        return -1;
    }

    fifo->data[fifo->tail] = new;
    fifo->tail = (fifo->tail + 1) % fifo->size;

    return 0;
}

int get_elem(FIFO *fifo) {
    if (is_empty(fifo)) {
        return -1;
    }

    int result = fifo->data[fifo->head];
    fifo->data[fifo->head] = -1;
    fifo->head = (fifo->head + 1) % fifo->size;

    return result;
}

void sobject_free(SObject *obj, bool delete) {
    if (shmdt(obj->ptr) == -1) {
        error(EXIT_FAILURE, errno, "shmdt");
    }

    if (!delete) return;

    if (shmctl(obj->id, IPC_RMID, NULL) == -1) {
        error(EXIT_FAILURE, errno, "shmctl");
    }

    free(obj);
}

void fifo_print(FIFO *fifo) {
    printf("s: %d, h: %d, t: %d\n", fifo->size, fifo->head, fifo->tail);
    for (int i = 0; i < fifo->size; i++) {
        printf("%d ", fifo->data[i]);
    }
    printf("\n");
}

void sem_signal(int semid, ushort semnum) {
    struct sembuf ops[1];
    ops[0].sem_num = semnum;
    ops[0].sem_op = 1;
    ops[0].sem_flg = 0;

    if (semop(semid, ops, 1) == -1) {
        error(EXIT_FAILURE, errno, "shmop");
    }
}

bool sem_wait(int semid, ushort semnum) {
    struct sembuf ops[1];
    ops[0].sem_num = semnum;
    ops[0].sem_op = -1;
    ops[0].sem_flg = 0;

    if (semop(semid, ops, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) {
            error(EXIT_FAILURE, errno, "shmop");
        } else {
            return false;
        }
    }

    return true;
}

void get_timestamp(struct timeval *start, struct timeval *result) {
    if (gettimeofday(result, NULL) == -1) {
        error(EXIT_FAILURE, errno, "gettimeofday");
    }

    __suseconds_t m = result->tv_usec - start->tv_usec;
    __time_t s = result->tv_sec - start->tv_sec;
    if (m < 0) {
        s -= 1;
        m += 1000000;
    }

    result->tv_sec = s;
    result->tv_usec = m;
}

void print_info(struct timeval *start, struct timeval *result, char *name, char *msg) {
    get_timestamp(start, result);
    printf("[%3ld.%.6ld] %s: %s\n", result->tv_sec, result->tv_usec, name, msg);
}
