#ifndef ZAD1_LIBBARBER_SHOP_H
#define ZAD1_LIBBARBER_SHOP_H

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

#define SEMAPHORES_KEY 's'
#define FIFO_KEY 'f'
#define BARBER_KEY 'b'
#define CLOCK_KEY 'c'

#define SO_MODE 0644

#define BARBER_READY 0
#define ACCESS_SEATS 1
#define CUSTOMER_READY 2

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
    int id;
    void *ptr;
} SObject;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

key_t key_get(int proj_id);

int semaphores_get(bool new);

SObject *barber_get(bool new);

SObject *clock_get(bool new);

SObject *fifo_get(bool new, int size);

bool is_empty(FIFO *fifo);

bool is_full(FIFO *fifo);

int put_elem(FIFO *fifo, int new);

int get_elem(FIFO *fifo);

void fifo_print(FIFO *fifo);

void sobject_free(SObject *obj, bool delete);

void sem_signal(int semid, ushort semnum);

bool sem_wait(int semid, ushort semnum);

void get_timestamp(struct timeval *start, struct timeval *result);

void print_info(struct timeval *start, struct timeval *result, char *name, char *msg);

#endif //ZAD1_LIBBARBER_SHOP_H
