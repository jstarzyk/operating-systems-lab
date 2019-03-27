#include "lib/libzad2.h"

#include <stdbool.h>
#include <signal.h>

#define MAX_CLIENT_MESSAGES 128

int client_queue_id;
char *client_queue_name;

void remove_queue() {
    if (mq_close(client_queue_id) == -1) {
        perror("mq_close");
        _exit(EXIT_FAILURE);
    }

    if (mq_unlink(client_queue_name) == -1) {
        perror("mq_unlink");
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

        send_message2(server_queue_id, type, pid, data, -1, generated_client_id);

        Message *received_msg = NULL;

        switch (type) {
            case MIRROR:
                printf("client %d sent MIRROR request (%d): %s\n", pid, i + 1, data);
                received_msg = wait_for_message(client_queue_id);
                printf("client %d received MIRROR result (%d): %s\n", pid, i + 1, received_msg->data);
                break;
            case CALC:
                printf("client %d sent CALC request (%d): %s\n", pid, i + 1, data);
                received_msg = wait_for_message(client_queue_id);
                printf("client %d received CALC result (%d): %lf\n", pid, i + 1, received_msg->result);
                break;
            case TIME:
                printf("client %d sent TIME request (%d)\n", pid, i + 1);
                received_msg = wait_for_message(client_queue_id);
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

    client_queue_name = get_queue_name(get_pid_max_len());
    printf("client queue name: %s\n", client_queue_name);

    int server_queue_id = get_queue(SERVER_QUEUE_NAME, O_WRONLY, 0644);
    printf("server_queue_id: %d\n", server_queue_id);
    client_queue_id = get_queue(client_queue_name, O_CREAT | O_RDWR, 0644);
    printf("client_queue_id: %d\n", client_queue_id);

    atexit(remove_queue);
    signal(SIGINT, handle_sigint);

    send_message2(server_queue_id, QUEUE_KEY_MSG, getpid(), client_queue_name, -1, client_queue_id);
    printf("client sent message with queue key: %d\n", client_queue_id);

    printf("client waiting for message with generated id...\n");
    Message *generated_client_id_msg = wait_for_message(client_queue_id);
    int generated_client_id = generated_client_id_msg->id;
    printf("client received generated id: %d\n", generated_client_id);

    send_requests(argv[1], server_queue_id, generated_client_id);
    free(generated_client_id_msg);

    send_message2(server_queue_id, CLOSE_QUEUE_MSG, getpid(), NULL, -1, generated_client_id);
    exit(EXIT_SUCCESS);
}
