#include "libbarber_shop.h"

sem_t **semaphores_get(bool new) {
    int flags = (new) ? O_CREAT : 0;

    sem_t **sem_set = malloc(3 * sizeof(sem_t *));
    sem_set[BARBER_READY] = sem_open(BARBER_READY_NAME, flags, SO_MODE, 0);
    sem_set[CUSTOMER_READY] = sem_open(CUSTOMER_READY_NAME, flags, SO_MODE, 0);
    sem_set[ACCESS_SEATS] = sem_open(ACCESS_SEATS_NAME, flags, SO_MODE, 1);

    for (int i = 0; i < 3; i++) {
        if (sem_set[i] == SEM_FAILED) {
            error(EXIT_FAILURE, errno, "semctl");
        }
    }

    return sem_set;
}

SObject *sobject_new(const char *name, size_t size, int flags) {
    int id = shm_open(name, flags, SO_MODE);
    if (id == -1) {
        error(EXIT_FAILURE, errno, "shm_open");
    }

    if (ftruncate(id, size) == -1) {
        error(EXIT_FAILURE, errno, "ftruncate");
    }

    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, id, 0);
    if (ptr == (void *) -1) {
        error(EXIT_FAILURE, errno, "mmap");
    }

    SObject *result = malloc(sizeof(SObject));
    result->name = name;
    result->ptr = ptr;
    result->size = size;

    return result;
}

SObject *barber_get(bool new) {
    int flags = (new) ? O_CREAT | O_RDWR : O_RDWR;

    SObject *result = sobject_new(BARBER_KEY, sizeof(BarberShop), flags);

    if (new) {
        BarberShop *barber_shop = (BarberShop *) result->ptr;
        barber_shop->is_open = true;
        barber_shop->barber_state = AWAKE;
    }

    return result;
}

SObject *clock_get(bool new) {
    int flags = (new) ? O_CREAT | O_RDWR : O_RDWR;

    SObject *result = sobject_new(CLOCK_KEY, sizeof(struct timeval), flags);

    if (new) {
        if (gettimeofday((struct timeval *) result->ptr, NULL) == -1) {
            error(EXIT_FAILURE, errno, "gettimeofday");
        }
    }

    return result;
}

SObject *fifo_get(bool new, int size) {
    int flags = (new) ? O_CREAT | O_RDWR : O_RDWR;

    SObject *result = sobject_new(FIFO_KEY, sizeof(FIFO), flags);

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
    if (munmap(obj->ptr, obj->size) == -1) {
        error(EXIT_FAILURE, errno, "munmap");
    }

    if (!delete) return;

    if (shm_unlink(obj->name) == -1) {
        error(EXIT_FAILURE, errno, "shm_unlink");
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

void _sem_signal(sem_t **semid, size_t semnum) {
    if (sem_post(semid[semnum]) == -1) {
        error(EXIT_FAILURE, errno, "sem_post");
    }
}

bool _sem_wait(sem_t **semid, size_t semnum) {
    if (sem_wait(semid[semnum]) == -1) {
        return false;
    } else {
        return true;
    }
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
