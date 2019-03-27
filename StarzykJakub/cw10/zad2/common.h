#ifndef ZAD1_COMMON_H
#define ZAD1_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <error.h>
#include <signal.h>

#define MAX_NAME_LENGTH 128
#define BUFFER_SIZE 1024

enum operation {
        ADD,
        SUB,
        MUL,
        DIV,
        REGISTRY,
        RESULT,
        ERROR,
        PING,
        PONG
};

struct __attribute__((__packed__)) {
        u_int8_t type;
//        u_int16_t size;
        int arg1;
        int arg2;
        int counter;
} *message = NULL;

struct __attribute__((__packed__)) message {
        u_int8_t type;
//        u_int16_t size;
//        void *content;
        int8_t content[BUFFER_SIZE];
};

static size_t max_path_len()
{
        struct sockaddr_un tmp;
        return sizeof(tmp.sun_path) - 1;
}

#endif //ZAD1_COMMON_H
