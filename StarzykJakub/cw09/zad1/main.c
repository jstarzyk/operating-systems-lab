#include <stdio.h>
#include <error.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <math.h>

#define CFG_BUFFER_SIZE 512

#define LT -1
#define EQ 0
#define GT 1

char **data;
volatile int tail = 0;
int head = 0;
int size;

int p, k, n, l, nk;
char *data_file;
int compare;
bool verbose;
FILE *input;
time_t start_time;
int n_len;
bool should_run = true;

pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct Line {
    int index;
    char *item;
} Line;

int items() {
    return (tail < head) ? (size - head + tail) : (tail - head);
}

void *producer(void *args) {
    pthread_t self = pthread_self();
    if (verbose) printf("[P-%ld] running...\n", self);

    char *line;
    size_t len = 0;
    ssize_t read_bytes;
    int res;

    while (true) {
        res = pthread_mutex_lock(&data_mutex);
        if (res != 0) error(EXIT_FAILURE, res, "pthread_mutex_lock");
        if (verbose) printf("[P-%ld] LOCK data_mutex\n", self);

        while (items() == n) {
            res = pthread_cond_wait(&full, &data_mutex);
            if (res != 0) error(EXIT_FAILURE, res, "pthread_cond_wait");
        }
        if (verbose) printf("[P-%ld] OWN data_mutex ON full\n", self);

        line = NULL;
        read_bytes = getline(&line, &len, input);

        if (read_bytes != -1) {
            if (verbose) printf("[P-%ld] PRODUCE %s", self, line);
        } else if (!feof(input)) {
            error(EXIT_FAILURE, errno, "getline");
        }

        data[tail] = line;
        tail = (tail + 1) % size;

        if (verbose) printf("[P-%ld] ADD %s", self, line);

        if (items() == 1) {
            res = pthread_cond_broadcast(&empty);
            if (res != 0) error(EXIT_FAILURE, res, "pthread_cond_broadcast");
            if (verbose) printf("[P-%ld] BROADCAST empty\n", self);
        }

        res = pthread_mutex_unlock(&data_mutex);
        if (res != 0) error(EXIT_FAILURE, res, "pthread_mutex_unlock");
        if (verbose) printf("[P-%ld] UNLOCK data_mutex\n", self);

        if (feof(input)) {
            should_run = false;
        }
    }
}

void print_line(Line *line, pthread_t id) {
    if (verbose) printf("[C-%ld] CONSUME %*d: %s", id, n_len, line->index, line->item);
    else printf("%s", line->item);
}

void *consumer(void *args) {
    pthread_t self = pthread_self();
    if (verbose) printf("[C-%ld] running...\n", self);

    Line line;
    int res;

    while (true) {
        res = pthread_mutex_lock(&data_mutex);
        if (res != 0) error(EXIT_FAILURE, res, "pthread_mutex_lock");
        if (verbose) printf("[C-%ld] LOCK data_mutex\n", self);

        while (items() == 0) {
            res = pthread_cond_wait(&empty, &data_mutex);
            if (res != 0) error(EXIT_FAILURE, res, "pthread_cond_wait");
        }
        if (verbose) printf("[C-%ld] OWN data_mutex ON empty\n", self);

        line.item = data[head];
        line.index = head;
        data[head] = NULL;
        head = (head + 1) % size;

        if (verbose) printf("[C-%ld] REMOVE %s", self, line.item);

        switch (compare) {
            case LT:
                if (strlen(line.item) < l) print_line(&line, self);
                break;
            case EQ:
                if (strlen(line.item) == l) print_line(&line, self);
                break;
            case GT:
                if (strlen(line.item) > l) print_line(&line, self);
                break;
            default:
                break;
        }

        if (items() == n - 1) {
            res = pthread_cond_broadcast(&full);
            if (res != 0) error(EXIT_FAILURE, res, "pthread_cond_broadcast");
            if (verbose) printf("[C-%ld] BROADCAST full\n", self);
        }

        res = pthread_mutex_unlock(&data_mutex);
        if (res != 0) error(EXIT_FAILURE, res, "pthread_mutex_unlock");
        if (verbose) printf("[C-%ld] UNLOCK data_mutex\n", self);
    }
}

void run() {
    int res, i;

    pthread_t producers[p];
    pthread_t consumers[k];

    start_time = time(NULL);

    for (i = 0; i < p; i++) {
        res = pthread_create(&producers[i], NULL, producer, NULL);
        if (res != 0) error(EXIT_FAILURE, res, "pthread_create");
    }

    for (i = 0; i < k; i++) {
        res = pthread_create(&consumers[i], NULL, consumer, NULL);
        if (res != 0) error(EXIT_FAILURE, res, "pthread_create");
    }

    if (nk > 0) {
        while (time(NULL) - start_time < nk) sleep(1);
    } else {
        while (should_run) usleep(1000);
    }

    for (i = 0; i < p; i++) {
        res = pthread_cancel(producers[i]);
        if (res != 0) error(EXIT_FAILURE, res, "pthread_cancel");
    }

    for (i = 0; i < k; i++) {
        res = pthread_cancel(consumers[i]);
        if (res != 0) error(EXIT_FAILURE, res, "pthread_cancel");
    }
}

char *parse_content(const char *file) {
    int fd = open(file, O_RDONLY);
    if (fd == -1) error(EXIT_FAILURE, errno, "open");

    char buffer[CFG_BUFFER_SIZE];
    size_t RESULT_SIZE = CFG_BUFFER_SIZE;
    size_t result_size = RESULT_SIZE;
    char *result = malloc(sizeof(char) * result_size);
    ssize_t bytes_read;
    ssize_t bytes_written = 0;

    while ((bytes_read = read(fd, buffer, CFG_BUFFER_SIZE)) > 0) {
        if (bytes_written + bytes_read > result_size) {
            result_size += RESULT_SIZE;
            result = realloc(result, result_size);
            if (result == NULL) {
                close(fd);
                return NULL;
            }
        }
        strncpy(&result[bytes_written], buffer, (size_t) bytes_read);
        bytes_written += bytes_read;
    }

    result[bytes_written - 1] = '\0';
    close(fd);

    return result;
}

void read_content(const char *content) {
    char delim[] = " \n\t";
    char *ptr;

    p = (int) strtoul(content, &ptr, 10);

    k = (int) strtoul(ptr, &ptr, 10);

    n = (int) strtoul(ptr, &ptr, 10);

    data_file = strtok_r(ptr, delim, &ptr);

    l = (int) strtoul(ptr, &ptr, 10);

    char op = strtok_r(ptr, delim, &ptr)[0];
    switch (op) {
        case '<':
            compare = LT;
            break;
        case '=':
            compare = EQ;
            break;
        case '>':
            compare = GT;
            break;
        default:
            break;
    }

    char *output_level = strtok_r(ptr, delim, &ptr);
    verbose = (strcmp(output_level, "verbose") == 0) ? true : false;

    nk = (int) strtoul(ptr, &ptr, 10);
}

void free_and_close() {
    for (int i = 0; i < size; i++) {
        if (data[i] != NULL) {
            free(data[i]);
        }
    }

    free(data);

    fclose(input);
}

void print_usage_and_exit() {
    printf("usage: <program> config_file\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 2) print_usage_and_exit();

    char *content = parse_content(argv[1]);
    if (content == NULL) {
        printf("invalid config file\n");
        exit(EXIT_FAILURE);
    }

    read_content(content);

    size = n + 1;

    data = malloc(size * sizeof(char *));
    for (int i = 0; i < size; i++) data[i] = NULL;

    atexit(free_and_close);

    input = fopen(data_file, "r");
    if (input == NULL) error(EXIT_FAILURE, errno, "fopen");

    free(content);

    n_len = (int) floor(log10(abs(n))) + 1;

    run();

    exit(EXIT_SUCCESS);
}
