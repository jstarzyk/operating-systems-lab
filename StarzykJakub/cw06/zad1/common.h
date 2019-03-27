#ifndef ZAD1_COMMON_H
#define ZAD1_COMMON_H

#include <unistd.h>

#define SERVER_ID 's'

#define MIRROR 1
#define CALC 2
#define TIME 3
#define END 4

#define GENERATED_CLIENT_ID_MSG 5
#define QUEUE_KEY_MSG 6

#define MAX_DATA_LENGTH 2048
#define MAX_CLIENTS 64

#define CLIENT 100
#define SERVER 10

struct Message {
    long type;
    pid_t pid;
    char data[MAX_DATA_LENGTH];
    double result;
    int id;
};
typedef struct Message Message;

#endif //ZAD1_COMMON_H
