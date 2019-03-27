#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include "lib/libzad1.h"

#define MAX_CLIENT_MESSAGES 128

int client_queue_id;

void remove_queue() {
    if (msgctl(client_queue_id, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        _exit(EXIT_FAILURE);
    }
}

long parse_message(char **line, char **result) {
    char *data = NULL;
    char *type_str = strtok_r(*line, " \n", &data);

    if (type_str == NULL) {
        *result = NULL;
        return -1;
    }

    if (data != NULL && data[strlen(data) - 1] == '\n') {
        data[strlen(data) - 1] = '\0';
    }

    long type;

    if (strcmp(type_str, "MIRROR") == 0) {
        type = MIRROR;
    } else if (strcmp(type_str, "CALC") == 0) {
        type = CALC;
    } else if (strcmp(type_str, "TIME") == 0) {
        type = TIME;
    } else if (strcmp(type_str, "END") == 0) {
        type = END;
    } else {
        *result = NULL;
        return -1;
    }

    *result = data;
    return type;
}

int get_lines(const char *file, char ***messages) {
    FILE *f = fopen(file, "r");

    if (f == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[7 + MAX_DATA_LENGTH];

    int i = 0;
    while (fgets(buffer, 7 + MAX_DATA_LENGTH, f) != NULL) {
        size_t line_size = strlen(buffer) + 1;
        char *line = malloc(line_size * sizeof(char));
        memcpy(line, buffer, line_size * sizeof(char));
        (*messages)[i++] = line;
    }

    fclose(f);
    return i;
}

void send_requests(char file[], int server_queue_id, int generated_client_id) {
    char **messages = malloc(MAX_CLIENT_MESSAGES * sizeof(char *));
    int n = get_lines(file, &messages);
    printf("lines: %d\n", n);
    char *data = NULL;
    bool sent_end_request = false;
    int pid = getpid();

    for (int i = 0; i < n && i < MAX_CLIENT_MESSAGES && !sent_end_request; i++) {
        long type = parse_message(&messages[i], &data);

        if (type == -1) {
            return;
        }

        send_message2(server_queue_id, 0, type, getpid(), data, -1, generated_client_id);

        Message *received_msg = NULL;

        switch (type) {
            case MIRROR:
                printf("client %d sent MIRROR request (%d): %s\n", pid, i + 1, data);
                received_msg = wait_for_message(client_queue_id, MIRROR, 0);
                printf("client %d received MIRROR result (%d): %s\n", pid, i + 1, received_msg->data);
                break;
            case CALC:
                printf("client %d sent CALC request (%d): %s\n", pid, i + 1, data);
                received_msg = wait_for_message(client_queue_id, CALC, 0);
                printf("client %d received CALC result (%d): %lf\n", pid, i + 1, received_msg->result);
                break;
            case TIME:
                printf("client %d sent TIME request (%d)\n", pid, i + 1);
                received_msg = wait_for_message(client_queue_id, TIME, 0);
                printf("client %d received TIME result (%d): %s\n", pid, i + 1, received_msg->data);
                break;
            case END:
                printf("client %d sent END request\n", pid);
                sent_end_request = true;
                break;
            default:
                break;
        }

        if (received_msg != NULL) {
            free(received_msg);
        }

        free(messages[i]);
    }

    free(messages);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: main path_to_file\n");
        exit(EXIT_FAILURE);
    }

    const char *home = getenv("HOME");

    client_queue_id = get_queue(CLIENT, 0666, home);
    printf("client_queue_id: %d\n", client_queue_id);
    int server_queue_id = get_queue(SERVER, 0666, home);
    printf("server_queue_id: %d\n", server_queue_id);

    atexit(remove_queue);
    signal(SIGINT, handle_sigint);

    send_message2(server_queue_id, 0, QUEUE_KEY_MSG, getpid(), NULL, -1, client_queue_id);
    printf("client sent message with queue key: %d\n", client_queue_id);

    printf("client waiting for message with generated id...\n");
    Message *generated_client_id_msg = wait_for_message(client_queue_id, GENERATED_CLIENT_ID_MSG, 0);
    int generated_client_id = generated_client_id_msg->id;
    printf("client received generated id: %d\n", generated_client_id);

    send_requests(argv[1], server_queue_id, generated_client_id);
    free(generated_client_id_msg);

    exit(EXIT_SUCCESS);
}
