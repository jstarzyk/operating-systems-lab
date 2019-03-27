#include "common.h"

#define UNIX_MAX_CLIENTS 16
#define INET_MAX_CLIENTS 16

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t expression_mutex = PTHREAD_MUTEX_INITIALIZER;

int unix_socket_fd;
int inet_socket_fd;
//int registry_epoll_fd;
//int task_epoll_fd;
int epoll_fd;

char *path;

struct {
        int type;
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

//        for(int i = 0; i < clients_counter; i++)
//                close(clients[i].fd);
//
//        close(task_epoll_fd);
//
//        shutdown(unix_socket_fd, SHUT_RDWR);
        close(unix_socket_fd);
        unlink(path);

//        shutdown(inet_socket_fd, SHUT_RDWR);
        close(inet_socket_fd);
        close(epoll_fd);

        if (message != NULL) {
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

int cmp_addr(struct sockaddr *a, struct sockaddr *b)
{
        if (a->sa_family == b->sa_family && a->sa_family == AF_UNIX) {
                struct sockaddr_un au = *(struct sockaddr_un *) a;
                struct sockaddr_un bu = *(struct sockaddr_un *) b;
                if (strcmp(au.sun_path, bu.sun_path) == 0)
                        return 1;
        } else if (a->sa_family == b->sa_family && a->sa_family == AF_INET) {
                struct sockaddr_in ai = *(struct sockaddr_in *) a;
                struct sockaddr_in bi = *(struct sockaddr_in *) b;
                if (ai.sin_port == bi.sin_port && ai.sin_addr.s_addr == bi.sin_addr.s_addr)
                        return 1;
        }
        return 0;
}

void reject_client(int type)
{
        ssize_t res;
        struct message msg;
        char error_msg[] = "name already taken";
        memset(&msg, 0, sizeof(struct message));
        msg.type = ERROR;
        strncpy(msg.content, error_msg, sizeof(msg.content) - 1);

        res = sendto(
                clients[clients_counter].type ? unix_socket_fd : inet_socket_fd,
                &msg,
                strlen(msg.content) + 2,
                MSG_WAITALL,
                (struct sockaddr *) &clients[clients_counter].addr,
                type ? sizeof(struct sockaddr_un) : sizeof(struct sockaddr_in)
        );
        if (res == -1)
                printf("unable to send error message: %s\n", strerror(errno));

        printf("client rejected\n");
}

void register_client()
{
        printf("client registered: %s\n", clients[clients_counter].name);
        clients_counter++;
}

void unregister_client(int index)
{
        printf("client unregistered: %s\n", clients[index].name);
        for (int i = index; i < clients_counter; i++) {
                clients[i] = clients[i + 1];
        }

        clients_counter--;
}

void handle_clients(char *name, struct sockaddr *addr, int type)
{
        int res;

        res = pthread_mutex_lock(&expression_mutex);
        if (res != 0)
                error(EXIT_FAILURE, res, "pthread_mutex_lock");

        clients[clients_counter].type = type;
        memcpy(&clients[clients_counter].addr, addr, type ? sizeof(struct sockaddr_un) : sizeof(struct sockaddr_in));
        strcpy(clients[clients_counter].name, name);


        int flag = 0;
        for (int i = 0; i < clients_counter; i++)
                if (strcmp(clients[i].name, name) == 0) {
                        reject_client(type);
                        flag = 1;
                }
        if (!flag) {
                register_client();
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
                        int index = rand() % clients_counter;
//                        printf("serv: a1:%d a2:%d\n", message->arg1, message->arg2);
                        bytes_sent = sendto(
                                clients[index].type ? unix_socket_fd : inet_socket_fd,
                                message,
                                sizeof(*message),
                                0,
                                (struct sockaddr *) &clients[index].addr,
                                clients[index].type ? sizeof(struct sockaddr_un) : sizeof(struct sockaddr_in)
                        );
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

        union {
                struct sockaddr_un unixAddr;
                struct sockaddr_in inetAddr;
        } addr;
        socklen_t len;
        memset(&addr, 0, sizeof(addr));

        res = epoll_wait(epoll_fd, &event, 1, 0);

        if (res == -1) {
                error(EXIT_FAILURE, errno, "epoll_wait");
        } else if (res == 1) {
                len = sizeof(addr);
                bytes_received = recvfrom(event.data.fd, &msg, BUFFER_SIZE + 1, MSG_WAITALL, (struct sockaddr *) &addr,
                                          &len);
                if (bytes_received > 0) {
                        int counter; long result;
                        switch (msg.type) {
                        case REGISTRY:
                                handle_clients(&msg.content, &addr, event.data.fd == unix_socket_fd ? 1 : 0);
                                break;
                        case RESULT:
                                counter = 1;
                                result = 2;
                                printf("result #%d: %ld\n", (int) ((long *) msg.content)[1], ((long *) msg.content)[0]);
                                break;
                        case PONG:
                                res = pthread_mutex_lock(&clients_mutex);
                                if (res != 0)
                                        error(EXIT_FAILURE, res, "pthread_mutex_lock");

                                for (int i = 0; i < clients_counter; i++) {
                                        if (cmp_addr((struct sockaddr *) &addr,
                                                     (struct sockaddr *) &clients[i].addr))
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

                        return;
                }
                if (bytes_received == 0)
                        printf("client has shut down\n");
                if (bytes_received == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
                        printf("unable to obtain result from client: %s\n", strerror(errno));

                res = pthread_mutex_lock(&clients_mutex);
                if (res != 0)
                        error(EXIT_FAILURE, res, "pthread_mutex_lock");

                for (int i = 0; i < clients_counter; i++)
                        if (cmp_addr((struct sockaddr *) &addr, (struct sockaddr *) &clients[i].addr))
                                unregister_client(i);

                res = pthread_mutex_unlock(&clients_mutex);
                if (res != 0)
                        error(EXIT_FAILURE, res, "pthread_mutex_unlock");
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
        uint8_t msg = PING;

        int clients_pinged;

        while (1) {
                pthread_mutex_lock(&clients_mutex);
                clients_pinged = clients_counter;
                for (int i = 0; i < clients_counter; i++) {
                        clients[i].status = 0;
                        result = sendto(
                                clients[i].type ? unix_socket_fd : inet_socket_fd,
                                &msg,
                                sizeof(msg),
                                MSG_WAITALL,
                                (struct sockaddr *) &clients[i].addr,
                                clients[i].type ? sizeof(struct sockaddr_un) : sizeof(struct sockaddr_in)
                        );
                        if (result == -1)
                                printf("unable to ping client %s: %s\n", clients[i].name, strerror(errno));
                }
                pthread_mutex_unlock(&clients_mutex);

                sleep(2);

                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < clients_pinged; i++) {
                        if (clients[i].status == 0)
                                unregister_client(i);
                }
                pthread_mutex_unlock(&clients_mutex);
        }
}

void setup_unix(const char *path)
{
        struct sockaddr_un server;

        unix_socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (unix_socket_fd == -1)
                error(EXIT_FAILURE, errno, "socket");
        memset(&server, 0, sizeof(struct sockaddr_un));
        server.sun_family = AF_UNIX;
        strncpy(server.sun_path, path, sizeof(server.sun_path) - 1);
        if (bind(unix_socket_fd, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) == -1)
                error(EXIT_FAILURE, errno, "bind");
//        if (listen(unix_socket_fd, UNIX_MAX_CLIENTS) == -1)
//                error(EXIT_FAILURE, errno, "listen");
}

void setup_inet(uint16_t port)
{
        struct sockaddr_in server;

        inet_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (inet_socket_fd == -1)
                error(EXIT_FAILURE, errno, "socket");
        memset(&server, 0, sizeof(struct sockaddr_in));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(port);
        if (bind(inet_socket_fd, (struct sockaddr *) &server, sizeof(struct sockaddr_in)) == -1)
                error(EXIT_FAILURE, errno, "bind");
//        if (listen(inet_socket_fd, INET_MAX_CLIENTS) == -1)
//                error(EXIT_FAILURE, errno, "listen");
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

//        task_epoll_fd = epoll_create1(0);
//        registry_epoll_fd = epoll_create1(0);
        epoll_fd = epoll_create1(0);

        union epoll_data unix_epoll_data;
        unix_epoll_data.fd = unix_socket_fd;
        struct epoll_event unix_epoll_event;
        unix_epoll_event.events = EPOLLIN;
        unix_epoll_event.data = unix_epoll_data;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, unix_socket_fd, &unix_epoll_event) == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");

        union epoll_data inet_epoll_data;
        inet_epoll_data.fd = inet_socket_fd;
        struct epoll_event inet_epoll_event;
        inet_epoll_event.events = EPOLLIN;
        inet_epoll_event.data = inet_epoll_data;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inet_socket_fd, &inet_epoll_event) == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");

        while (1) {
//                handle_clients();
                send_message();
                handle_result();
                usleep(1000);
        }
}
