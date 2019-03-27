#ifndef ZAD2_LIBBARBER_SHOP_H
#define ZAD2_LIBBARBER_SHOP_H

#include <sys/ipc.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <error.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define FIFO_KEY "fifo"
#define BARBER_KEY "barber"
#define CLOCK_KEY "clock"

#define SO_MODE 0644

#define BARBER_READY 0
#define BARBER_READY_NAME "barber"
#define ACCESS_SEATS 1
#define ACCESS_SEATS_NAME "wr"
#define CUSTOMER_READY 2
#define CUSTOMER_READY_NAME "customer"

#define SLEEPING 0
#define AWAKE 1
#define CUTTING_HAIR 2
#define DONE 3

#define MAX_WR_SEATS 31

typedef struct FIFO {
    int size;
    int data[MAX_WR_SEATS + 1];
    int head;
    int tail;
} FIFO;

typedef struct BarberShop {
    bool is_open;
    int barber_state;
    int wr_seats;
    int immediate_customer_pid;
} BarberShop;

typedef struct SharedObject {
    const char *name;
    void *ptr;
    size_t size;
} SObject;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

sem_t **semaphores_get(bool new);

SObject *barber_get(bool new);

SObject *clock_get(bool new);

SObject *fifo_get(bool new, int size);

bool is_empty(FIFO *fifo);

bool is_full(FIFO *fifo);

int put_elem(FIFO *fifo, int new);

int get_elem(FIFO *fifo);

void fifo_print(FIFO *fifo);

void sobject_free(SObject *obj, bool delete);

void _sem_signal(sem_t **semid, size_t semnum);

bool _sem_wait(sem_t **semid, size_t semnum);

void get_timestamp(struct timeval *start, struct timeval *result);

void print_info(struct timeval *start, struct timeval *result, char *name, char *msg);

#endif //ZAD2_LIBBARBER_SHOP_H
