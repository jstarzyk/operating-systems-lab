#include "common.h"

#define UNIX_MAX_CLIENTS 16
#define INET_MAX_CLIENTS 16

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t expression_mutex = PTHREAD_MUTEX_INITIALIZER;

int unix_socket_fd;
int inet_socket_fd;
int registry_epoll_fd;
int task_epoll_fd;

char *path;

struct {
        int fd;
        char name[MAX_NAME_LENGTH];
        union {
                struct sockaddr_un unix_addr;
                struct sockaddr_in inet_addr;
        } addr;
        int status;
} clients[UNIX_MAX_CLIENTS + INET_MAX_CLIENTS];

int clients_counter = 0;

pthread_t ping_thread;
pthread_t input_thread;

void clean_up()
{
        pthread_mutex_destroy(&clients_mutex);
        pthread_mutex_destroy(&expression_mutex);

        for(int i = 0; i < clients_counter; i++)
                close(clients[i].fd);

        close(task_epoll_fd);

        shutdown(unix_socket_fd, SHUT_RDWR);
        close(unix_socket_fd);
        unlink(path);

        shutdown(inet_socket_fd, SHUT_RDWR);
        close(inet_socket_fd);

        if (message != NULL){
                free(message);
        }
}

void handle_sigint(int signo)
{
        exit(EXIT_SUCCESS);
}

int parse_expression(const char *buffer)
{
        if (buffer == NULL)
                return -1;

        char *next, *type;
        long arg;

        arg = strtol(buffer, &next, 10);
        if (arg < INT32_MIN || arg > INT32_MAX)
                return -1;
        message->arg1 = (int) arg;

        if (next == NULL)
                return -1;

        type = strtok_r(next, "0123456789 \n\t", &next);
        if (type == NULL || strlen(type) > 1)
                return -1;

        switch (type[0]) {
        case '+':
                message->type = ADD;
                break;
        case '-':
                message->type = SUB;
                break;
        case '*':
                message->type = MUL;
                break;
        case '/':
                message->type = DIV;
                break;
        default:
                return -1;
        }

        if (next == NULL)
                return -1;

        arg = strtol(next, &next, 10);
        if (arg < INT32_MIN || arg > INT32_MAX)
                return -1;
        message->arg2 = (int) arg;

        return 0;
}

int set_name()
{
        ssize_t res;
        struct message msg;

        res = recv(clients[clients_counter].fd, &msg, 3, MSG_WAITALL);

        if (res == 3 && msg.type == NAME) {
                msg.content = malloc(msg.size);
                res = recv(clients[clients_counter].fd, msg.content, msg.size, MSG_WAITALL);

                if (res == msg.size) {
                        strncpy(clients[clients_counter].name, msg.content, MAX_NAME_LENGTH - 1);

                        for (int i = 0; i < clients_counter; i++) {
                                if (strcmp(clients[i].name, msg.content) == 0) {
                                        free(msg.content);
                                        return 0;
                                }
                        }

                        free(msg.content);
                        return 1;
                }
        }

        if (res == 0)
                printf("client has shut down during registration\n");
        else if (res == -1)
                printf("unable to obtain client's name: %s\n", strerror(errno));

        close(clients[clients_counter].fd);
        return -1;
}

void reject_client()
{
        ssize_t res;
        struct __attribute__((__packed__)) {
                uint8_t type;
                uint16_t size;
        } msg;
        char error_msg[] = "name already taken";

        msg.type = ERROR;
        msg.size = (uint16_t) (strlen(error_msg) + 1);

        res = send(clients[clients_counter].fd, &msg, sizeof(msg), MSG_MORE);
        if (res == -1)
                printf("unable to send message header: %s\n", strerror(errno));
        res = send(clients[clients_counter].fd, error_msg, sizeof(error_msg), 0);
        if (res == -1)
                printf("unable to send error message: %s\n", strerror(errno));

        close(clients[clients_counter].fd);
        printf("client rejected\n");
}

void register_client()
{
        int res;
        union epoll_data data;
        struct epoll_event event;

        data.fd = clients[clients_counter].fd;
        event.events = EPOLLIN;
        event.data = data;
        res = epoll_ctl(task_epoll_fd, EPOLL_CTL_ADD, clients[clients_counter].fd, &event);
        if (res == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");
        printf("client registered: %s\n", clients[clients_counter].name);
        clients_counter++;
}

void unregister_client(int fd)
{
//        int fd = clients[fd].fd;
//        char *name = clients[fd].name;
        int res;

        res = epoll_ctl(task_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        if (res == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");

        res = pthread_mutex_lock(&clients_mutex);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_mutex_lock");

        for (int i = 0, found = 0; i < clients_counter; i++) {
                if (found) clients[i - 1] = clients[i];
                else if (clients[i].fd == fd) {
                        found = 1;
                        printf("client unregistered: %s\n", clients[i].name);
                }
        }

        res = pthread_mutex_unlock(&clients_mutex);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_mutex_unlock");

        clients_counter--;
        close(fd);
}

void handle_clients()
{
        int res;
        struct epoll_event event;
        socklen_t length;

        res = pthread_mutex_lock(&expression_mutex);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_mutex_lock");

        res = epoll_wait(registry_epoll_fd, &event, 1, 0);
        if (res == -1) {
                error(EXIT_FAILURE, errno, "epoll_wait");
        } else if (res == 1) {
                if (event.data.fd == unix_socket_fd)
                        length = sizeof(struct sockaddr_un);
                else
                        length = sizeof(struct sockaddr_in);

                clients[clients_counter].fd = accept(event.data.fd, (struct sockaddr *) &clients[clients_counter].addr,
                                                     &length);

                if (clients[clients_counter].fd != -1) {
                        res = set_name();
                        if (res == 0)
                                reject_client();
                        else if (res == 1)
                                register_client();
                } else if (clients[clients_counter].fd == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        error(EXIT_FAILURE, errno, "accept");
                }
        }

        res = pthread_mutex_unlock(&expression_mutex);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_mutex_unlock");
}

void send_message()
{
        int res;
        ssize_t bytes_sent;

        res = pthread_mutex_lock(&clients_mutex);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_mutex_lock");

        if (clients_counter > 0) {
                res = pthread_mutex_lock(&expression_mutex);
                if (res != 0)
                        error(EXIT_FAILURE, res, "pthread_mutex_lock");

                if (message != NULL) {
                        bytes_sent = send(clients[rand() % clients_counter].fd, message, sizeof(*message), 0);
                        if (bytes_sent == sizeof(*message)) {
                                free(message);
                                message = NULL;

                        } else {
                                printf("unable to send message: %s\n", strerror(errno));
                        }
                }

                res = pthread_mutex_unlock(&expression_mutex);
                if (res != 0)
                        error(EXIT_FAILURE, res, "pthread_mutex_unlock");
        }

        res = pthread_mutex_unlock(&clients_mutex);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_mutex_unlock");
}

void handle_result()
{
        ssize_t bytes_received;
        int res;

        struct message msg;
        struct epoll_event event;

        res = epoll_wait(task_epoll_fd, &event, 1, 0);
        if (res == -1) {
               error(EXIT_FAILURE, errno, "epoll_wait");
        } else if (res == 1) {
                bytes_received = recv(event.data.fd, &msg, 3, MSG_DONTWAIT);
                if (bytes_received == 3) {
                        msg.content = malloc(msg.size);

                        if (msg.size != 0)
                                bytes_received = recv(event.data.fd, msg.content, msg.size, MSG_WAITALL);
                        if (bytes_received == msg.size || msg.size == 0) {
                                switch (msg.type) {
                                case RESULT:
                                        printf("result #%d: %ld\n", ((int*)msg.content)[2], *(long *) msg.content);
                                        break;
                                case PONG:
                                        res = pthread_mutex_lock(&clients_mutex);
                                        if (res != 0)
                                                error(EXIT_FAILURE, res, "pthread_mutex_lock");

                                        for (int i = 0; i < clients_counter; i++) {
                                                if (clients[i].fd == event.data.fd)
                                                        clients[i].status = 1;
                                        }

                                        res = pthread_mutex_unlock(&clients_mutex);
                                        if (res != 0)
                                                error(EXIT_FAILURE, res, "pthread_mutex_unlock");
                                        break;
                                case ERROR:
                                        printf("error: %s\n", (char *) msg.content);
                                        break;
                                default:
                                        break;
                                }

                                free(msg.content);
                                return;
                        }
                        free(msg.content);
                }
                if (bytes_received == 0)
                        printf("client has shut down\n");
                if (bytes_received == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
                        printf("unable to obtain result from client: %s\n", strerror(errno));
                unregister_client(event.data.fd);
        }
}

void *input_task(void *args)
{
        char *buffer;
        size_t len = 0;
        int res;
        int counter = 0;

        while (1) {
                buffer = NULL;
                if (getline(&buffer, &len, stdin) == -1)
                        error(EXIT_FAILURE, errno, "getline");

                res = pthread_mutex_lock(&expression_mutex);
                if (res != 0)
                        error(EXIT_FAILURE, res, "pthread_mutex_lock");

                message = malloc(sizeof(*message));
                message->size = 3 * sizeof(int);

                if (parse_expression(buffer) == -1) {
                        printf("invalid expression\n");
                        free(message);
                        message = NULL;
                        pthread_mutex_unlock(&expression_mutex);
                        continue;
                }

                message->counter = counter++;

                res = pthread_mutex_unlock(&expression_mutex);
                if (res != 0)
                        error(EXIT_FAILURE, res, "pthread_mutex_unlock");

                free(buffer);
        }
}

void *ping_task(void *args)
{
        ssize_t result;
        struct __attribute__((__packed__)) {
                uint8_t type;
                uint16_t size;
        } msg;
        msg.type = PING;
        msg.size = 0;

        int clients_pinged;

        while (1) {
                pthread_mutex_lock(&clients_mutex);
                clients_pinged = clients_counter;
                for (int i = 0; i < clients_counter; i++) {
                        clients[i].status = 0;
                        result = send(clients[i].fd, &msg, sizeof(msg), MSG_WAITALL);
                        if (result == -1)
                                printf("unable to ping client %s: %s\n", clients[i].name, strerror(errno));
                }
                pthread_mutex_unlock(&clients_mutex);

                sleep(2);

                for (int i = 0; i < clients_pinged; i++) {
                        if (clients[i].status == 0)
                                unregister_client(clients[i].fd);
                }
        }
}

void setup_unix(const char *path)
{
        struct sockaddr_un server;

        unix_socket_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (unix_socket_fd == -1)
                error(EXIT_FAILURE, errno, "socket");
//        if (unlink(path) == -1)
//                error(EXIT_FAILURE, errno, "unlink");
        memset(&server, 0, sizeof(struct sockaddr_un));
        server.sun_family = AF_UNIX;
        strncpy(server.sun_path, path, sizeof(server.sun_path) - 1);
        if (bind(unix_socket_fd, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) == -1)
                error(EXIT_FAILURE, errno, "bind");
        if (listen(unix_socket_fd, UNIX_MAX_CLIENTS) == -1)
                error(EXIT_FAILURE, errno, "listen");
}

void setup_inet(uint16_t port)
{
        struct sockaddr_in server;

        inet_socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (inet_socket_fd == -1)
                error(EXIT_FAILURE, errno, "socket");
        memset(&server, 0, sizeof(struct sockaddr_in));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(port);
        if (bind(inet_socket_fd, (struct sockaddr *) &server, sizeof(struct sockaddr_in)) == -1)
                error(EXIT_FAILURE, errno, "bind");
        if (listen(inet_socket_fd, INET_MAX_CLIENTS) == -1)
                error(EXIT_FAILURE, errno, "listen");
}


void print_usage_and_exit()
{
        printf("usage: <program> <port number> <socket path>\n");
        printf("\t1024 <= <port number> <= %d\n", UINT16_MAX);
        printf("\t1 <= length of <socket path> <= %zu\n", max_path_len());
        exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
        signal(SIGINT, handle_sigint);
        atexit(clean_up);

        if (argc != 3)
                print_usage_and_exit();

        long port_tmp = strtol(argv[1], NULL, 10);
        if (port_tmp < 1024 || port_tmp > UINT16_MAX)
                print_usage_and_exit();
        uint16_t port = (uint16_t) port_tmp;

        path = argv[2];
        if (strlen(path) > max_path_len())
                print_usage_and_exit();

        setup_inet(port);
        setup_unix(path);

        int res;

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        res = pthread_create(&ping_thread, &attr, ping_task, NULL);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_create");
        res = pthread_create(&input_thread, &attr, input_task, NULL);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_create");
        pthread_attr_destroy(&attr);

        task_epoll_fd = epoll_create1(0);
        registry_epoll_fd = epoll_create1(0);

        union epoll_data unix_epoll_data;
        unix_epoll_data.fd = unix_socket_fd;
        struct epoll_event unix_epoll_event;
        unix_epoll_event.events = EPOLLIN;
        unix_epoll_event.data = unix_epoll_data;
        if (epoll_ctl(registry_epoll_fd, EPOLL_CTL_ADD, unix_socket_fd, &unix_epoll_event) == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");

        union epoll_data inet_epoll_data;
        inet_epoll_data.fd = inet_socket_fd;
        struct epoll_event inet_epoll_event;
        inet_epoll_event.events = EPOLLIN;
        inet_epoll_event.data = inet_epoll_data;
        if (epoll_ctl(registry_epoll_fd, EPOLL_CTL_ADD, inet_socket_fd, &inet_epoll_event) == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");

        while (1) {
                handle_clients();
                send_message();
                handle_result();
                usleep(1000);
        }
}
