#include "libzad1.h"

#include <sys/msg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

const size_t message_data_size = sizeof(Message) - sizeof(long);

void send_message2(int queue_id, int flags, long type, pid_t pid, char *data, double result, int id) {
    Message *created = malloc(sizeof(Message));
    created->type = type;
    created->pid = pid;
    created->result = result;
    created->id = id;

    if (data != NULL) {
        strncpy(created->data, data, MAX_DATA_LENGTH);
    }

    if (msgsnd(queue_id, created, message_data_size, flags) == -1) {
        if (errno != EINVAL) {
            perror("msgsnd");
        } else {
            printf("queue %d removed\n", queue_id);
        }
        exit(EXIT_FAILURE);
    }

    free(created);
}

Message *wait_for_message(int queue_id, long type, int flags) {
    Message *received = malloc(sizeof(Message));

    if (msgrcv(queue_id, received, message_data_size, type, flags) == -1) {
        if (errno != EINVAL) {
            perror("msgrcv");
        } else {
            printf("queue %d removed\n", queue_id);
        }
        exit(EXIT_FAILURE);
    }

    return received;
}

key_t get_key(const char *path, char id) {
    int key = ftok(path, id);

    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    return key;
}

int get_queue(int role, int flags, const char *path) {
    key_t key = (role == CLIENT) ? IPC_PRIVATE : get_key(path, SERVER_ID);

    int queue_id = msgget(key, flags);

    if (queue_id == -1) {
        if (errno != ENOENT) {
            perror("msgget");
        } else {
            printf("queue does not exist\n");
        }
        exit(EXIT_FAILURE);
    }

    return queue_id;
}

void handle_sigint(int signo) {
    exit(EXIT_FAILURE);
}