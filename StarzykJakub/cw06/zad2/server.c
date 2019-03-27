#include "lib/libzad2.h"

#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <signal.h>

int server_queue_id;
int n_clients = 0;

struct Client {
    int pid;
    int queue_id;
    bool closed;
};
typedef struct Client Client;

Client clients[MAX_CLIENTS];

char *reverse_string(char *str, size_t len) {
    char *p1 = str;
    char *p2 = str + len - 1;

    while (p1 < p2) {
        char tmp = *p1;
        *p1++ = *p2;
        *p2-- = tmp;
    }

    return str;
}

void mirror_service(Message *client_msg, Client clients[]) {
    char *reversed = reverse_string(client_msg->data, strlen(client_msg->data));
    send_message2(clients[client_msg->id].queue_id, MIRROR, getpid(), reversed, -1, -1);
}

bool parse_operand(char *op, double *res) {
    char *endptr;

    double parsed = strtod(op, &endptr);
    if (errno == ERANGE || (parsed == 0 && strcmp(op, endptr) == 0)) {
        return false;
    }

    *res = parsed;
    return true;
}

double parse_and_calculate(char *calc) {
    if (calc == NULL) {
        return NAN;
    }

    char *instr = strtok(calc, " ");
    char *op1 = strtok(NULL, " ");
    char *op2 = strtok(NULL, " ");

    if (instr == NULL || op1 == NULL || op2 == NULL) {
        return NAN;
    }

    double result;
    double res1, res2;

    if (!parse_operand(op1, &res1) || !parse_operand(op2, &res2)) {
        return NAN;
    }

    if (strcmp(instr, "ADD") == 0) {
        result = res1 + res2;
    } else if (strcmp(instr, "MUL") == 0) {
        result = res1 * res2;
    } else if (strcmp(instr, "SUB") == 0) {
        result = res1 - res2;
    } else if (strcmp(instr, "DIV") == 0) {
        result = res1 / res2;
    } else {
        return NAN;
    }

    return result;
}

void calc_service(Message *client_msg, Client clients[]) {
    double result = parse_and_calculate(client_msg->data);
    send_message2(clients[client_msg->id].queue_id, CALC, getpid(), NULL, result, -1);
}

void time_service(Message *client_msg, Client clients[]) {
    char buffer[30];
    time_t current_time = time(NULL);
    strftime(buffer, sizeof(buffer), "%F %T", localtime(&current_time));
    send_message2(clients[client_msg->id].queue_id, TIME, getpid(), buffer, -1, -1);
}

long remaining_messages = 0;
bool server_should_wait_for_new_messages = true;

void do_not_wait_for_new_messages(int queue_id) {
    server_should_wait_for_new_messages = false;
    struct mq_attr buf;

    if (mq_getattr(queue_id, &buf) == -1) {
        perror("mq_getattr");
        exit(EXIT_FAILURE);
    }

    remaining_messages = buf.mq_curmsgs;
}

bool server_should_process_messages() {
    return server_should_wait_for_new_messages || (remaining_messages > 0);
}

void remove_queue() {
    for (int i = 0; i < n_clients; i++) {
        if (!clients[i].closed) {
            if (mq_close(clients[i].queue_id) == -1) {
                perror("mq_close");
                _exit(EXIT_FAILURE);
            }
        }
    }

    if (mq_close(server_queue_id) == -1) {
        perror("mq_close");
        _exit(EXIT_FAILURE);
    }

    if (mq_unlink(SERVER_QUEUE_NAME) == -1) {
        perror("mq_unlink");
        _exit(EXIT_FAILURE);
    }
}

int main() {
    server_queue_id = get_queue(SERVER_QUEUE_NAME, O_CREAT | O_RDWR, 0644);
    printf("%d\n", server_queue_id);

    atexit(remove_queue);
    signal(SIGINT, handle_sigint);

    int clients_index = 0;

    while (server_should_process_messages()) {
        Message *client_msg = wait_for_message(server_queue_id);

        if (!server_should_wait_for_new_messages) remaining_messages--;

        if (client_msg->type == QUEUE_KEY_MSG) {
            if (clients_index >= MAX_CLIENTS) {
                kill(client_msg->pid, SIGINT);
                continue;
            }

            clients[clients_index].pid = client_msg->pid;
            clients[clients_index].queue_id = get_queue(client_msg->data, O_RDWR, 0644);
            clients[clients_index].closed = false;
            send_message2(clients[clients_index].queue_id, GENERATED_CLIENT_ID_MSG, getpid(), NULL, -1, clients_index);

            clients_index++;
            n_clients = clients_index;
        } else if (client_msg->type == CLOSE_QUEUE_MSG) {
            if (mq_close(clients[client_msg->id].queue_id) == -1) {
                perror("mq_close");
                _exit(EXIT_FAILURE);
            }

            clients[client_msg->id].closed = true;
        } else {
            switch (client_msg->type) {
                case MIRROR:
                    mirror_service(client_msg, clients);
                    break;
                case CALC:
                    calc_service(client_msg, clients);
                    break;
                case TIME:
                    time_service(client_msg, clients);
                    break;
                case END:
                    do_not_wait_for_new_messages(server_queue_id);
                    printf("%ld messages remaining\n", remaining_messages);
                    break;
                default:
                    break;
            }
        }

        free(client_msg);
    }

    exit(EXIT_SUCCESS);
}
