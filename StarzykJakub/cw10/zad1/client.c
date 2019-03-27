#include "common.h"
//#include <memory.h>

int socket_fd;

void clean_up()
{
        shutdown(socket_fd, SHUT_RDWR);
        close(socket_fd);
}

void handle_sigint(int signo)
{
        exit(EXIT_SUCCESS);
}

void print_usage_and_exit()
{
        printf("usage: <program> <client name> <connection type> <address>\n");
        printf("\t1 <= length of <client name> <= %d\n", MAX_NAME_LENGTH - 1);
        printf("\t<connection type>: {inet, unix}\n");
        printf("\t<address>: {<IPv4> <port number>, <socket path>}");
        exit(EXIT_FAILURE);
}

void send_name(const char *client_name, int socket_fd)
{
        struct __attribute__((__packed__)) {
                uint8_t type;
                uint16_t size;
        } msg;
        msg.type = NAME;
        msg.size = (uint16_t) (strlen(client_name) + 1);

        if (send(socket_fd, &msg, 3, MSG_MORE) == -1)
                error(EXIT_FAILURE, errno, "send");
        if (send(socket_fd, client_name, msg.size, 0) == -1)
                error(EXIT_FAILURE, errno, "send");
}

int main(int argc, char *argv[])
{
        signal(SIGINT, handle_sigint);
        atexit(clean_up);

        if (argc < 4 || argc > 5)
                print_usage_and_exit();

        long res;
        char *client_name;
        int connection_type;
        union {
                struct sockaddr_un unix_addr;
                struct sockaddr_in inet_addr;
        } addr;

        client_name = argv[1];
        if (strlen(client_name) > MAX_NAME_LENGTH - 1)
                print_usage_and_exit();

        if (argc == 4) {
                if (strcmp(argv[2], "unix") == 0)
                        connection_type = 0;
                else
                        print_usage_and_exit();
                memset(&addr.unix_addr, 0, sizeof(struct sockaddr_un));
                addr.unix_addr.sun_family = AF_UNIX;
                strncpy(addr.unix_addr.sun_path, argv[3], sizeof(addr.unix_addr.sun_path) - 1);
        } else if (argc == 5) {
                if (strcmp(argv[2], "inet") == 0)
                        connection_type = 1;
                else
                        print_usage_and_exit();
                addr.inet_addr.sin_family = AF_INET;
                res = inet_pton(AF_INET, argv[3], &addr.inet_addr.sin_addr);
                if (res == -1)
                        print_usage_and_exit();

                long port_tmp = strtol(argv[4], NULL, 10);
                if (port_tmp < 1024 || port_tmp > UINT16_MAX)
                        print_usage_and_exit();
                addr.inet_addr.sin_port = htons((uint16_t) port_tmp);
        }

        socket_fd = socket(connection_type ? AF_INET : AF_UNIX, SOCK_STREAM, 0);
        if (socket_fd == -1)
                error(EXIT_FAILURE, errno, "socket");
        res = connect(socket_fd, (struct sockaddr *) &addr, connection_type ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_un));
        if (res < 0)
                error(EXIT_FAILURE, errno, "connect");


        send_name(client_name, socket_fd);

        struct message msg;
        long result;

        while (1) {
                res = recv(socket_fd, &msg, 3, MSG_WAITALL);
                if (res == 3) {
                        msg.content = malloc(msg.size);
                        if (msg.size != 0)
                                res = recv(socket_fd, msg.content, msg.size, MSG_WAITALL);
                        if (res == msg.size || msg.size == 0) {
                                switch (msg.type) {
                                case ADD:
                                        result = ((int *) msg.content)[0] + ((int *) msg.content)[1];
                                        break;
                                case SUB:
                                        result = ((int *) msg.content)[0] - ((int *) msg.content)[1];
                                        break;
                                case MUL:
                                        result = ((int *) msg.content)[0] * ((int *) msg.content)[1];
                                        break;
                                case DIV:
                                        result = ((int *) msg.content)[0] / ((int *) msg.content)[1];
                                        break;
                                case PING: {
                                        struct __attribute__((__packed__)) {
                                                uint8_t type;
                                                uint16_t size;
                                        } pong_msg;
                                        pong_msg.type = PONG;
                                        pong_msg.size = 0;
                                        res = send(socket_fd, &pong_msg, 3, MSG_WAITALL);
                                        if (res != 3)
                                                error(EXIT_FAILURE, errno, "send");
                                        continue;
                                }
                                case ERROR:
                                        printf("received error message from server: %s\n", (char *) msg.content);
                                        exit(EXIT_FAILURE);
                                default:
                                        exit(EXIT_FAILURE);
                                }

                                struct __attribute__((__packed__)) {
                                        u_int8_t type;
                                        u_int16_t size;
                                        long result;
                                        int counter;
                                } result_msg;

                                result_msg.type = RESULT;
                                result_msg.size = sizeof(long) + sizeof(int);
                                result_msg.result = result;
                                result_msg.counter = ((int *) msg.content)[2];

                                res = send(socket_fd, &result_msg, sizeof(result_msg), MSG_WAITALL);
                                if (res == -1)
                                        error(EXIT_FAILURE, errno, "send");
                        }
                        free(msg.content);
                }
                if (res == 0) {
                        printf("server has shut down\n");
                        exit(EXIT_FAILURE);
                }
                if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        printf("unable to obtain message from server: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }
}
