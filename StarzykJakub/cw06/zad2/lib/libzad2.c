#include "libzad2.h"

const size_t message_data_size = sizeof(Message);

void send_message2(int queue_id, long type, pid_t pid, char *data, double result, int id) {
    Message *created = malloc(sizeof(Message));
    created->type = type;
    created->pid = pid;
    created->result = result;
    created->id = id;

    if (data != NULL) {
        strncpy(created->data, data, MAX_DATA_LENGTH);
    }

    if (mq_send(queue_id, (char *) created, message_data_size, 0) == -1) {
        if (errno != EINVAL) {
            perror("mq_send");
        } else {
            printf("queue %d removed\n", queue_id);
        }
        printf("message_data_size: %zu\n", message_data_size);
        exit(EXIT_FAILURE);
    }

    free(created);
}

Message *wait_for_message(int queue_id) {
    Message *received = malloc(sizeof(Message));

    if (mq_receive(queue_id, (char *) received, 8192, NULL) == -1) {
        if (errno != EINVAL) {
            perror("mq_receive");
        } else {
            printf("queue %d removed\n", queue_id);
        }
        exit(EXIT_FAILURE);
    }

    return received;
}

size_t get_pid_max_len() {
    FILE *p = fopen("/proc/sys/kernel/pid_max", "r");

    char *pid_max_str = malloc(100 * sizeof(char));
    if (p != NULL) {
        pid_max_str = fgets(pid_max_str, 100, p);
    }

    if (pid_max_str[strlen(pid_max_str) - 1] == '\n') {
        pid_max_str[strlen(pid_max_str) - 1] = '\0';
    }

    fclose(p);
    return strlen(pid_max_str);
}

char *get_queue_name(size_t pid_max_len) {
    char *queue_name = malloc((3 + pid_max_len) * sizeof(char));
    sprintf(queue_name, "/mq%d", getpid());
    return queue_name;
}

int get_queue(const char *queue_name, int flags, mode_t mode) {
    int queue_id;

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_curmsgs = 0;
    attr.mq_msgsize = 4096;

    queue_id = mq_open(queue_name, flags, mode, &attr);

    if (queue_id == -1) {
        if (errno != ENOENT) {
            perror("mq_open");
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
