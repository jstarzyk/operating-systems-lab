#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h>

#define BUFFER_SIZE 8192
#define MAX_GRAY_VALUE 255


typedef struct PortableGrayMap {
    int width;
    int height;
    u_char max;
    u_char **data;
} PGM;

typedef struct Filter {
    int size;
    double **data;
} Filter;

typedef struct Lines {
    int from;
    int to;
} Lines;

PGM *input_image;
PGM *output_image;
Filter *global_filter;


void invalid_file() {
    printf("invalid ASCII PGM file\n");
    exit(EXIT_FAILURE);
}

void pgm_free(PGM *image) {
    for (int i = 0; i < image->height; i++) free(image->data[i]);
    free(image->data);
    free(image);
}

void filter_free(Filter *filter) {
    for (int i = 0; i < filter->size; i++) free(filter->data[i]);
    free(filter->data);
    free(filter);
}

static int max(const int a, const int b) {
    return a > b ? a : b;
}

static int min(const int a, const int b) {
    return a > b ? b : a;
}

void *thread_start(void *lines) {
    int c = global_filter->size;
    int neg_c2 = -(int) ceil(c / 2.0);

    int from = ((Lines *) lines)->from;
    int to = ((Lines *) lines)->to;

    int width = input_image->width;
    int height = input_image->height;

    u_char **input_data = input_image->data;
    u_char **output_data = &output_image->data[from - 1];
    double **filter_data = global_filter->data;

    u_char *max_val = malloc(sizeof(u_char));
    u_char fval;

    double s;
    int _x, _y;

    u_char *input_line;
    double *filter_line;

    _y = from - 1 + neg_c2;
    for (int y = 0; y <= to - from; y++) {
        _y++;
        _x = neg_c2;
        for (int x = 0; x < width; x++) {
            _x++;
            s = 0.0;

            for (int j = 1; j <= c; j++) {
                input_line = input_data[min(height, max(1, _y + j)) - 1];
                filter_line = filter_data[j - 1];

                for (int i = 1; i <= c; i++) {
                    s += input_line[min(width, max(1, _x + i)) - 1] * filter_line[i - 1];
                }
            }

            fval = (u_char) round(s);
            if (fval > *max_val) *max_val = fval;

            output_data[y][x] = fval;
        }
    }

    return max_val;
}

PGM *filter_image(PGM *image, Filter *filter, int num_threads) {
    if (num_threads < 1) return NULL;

    input_image = image;
    global_filter = filter;
    int width = input_image->width;
    int height = input_image->height;

    u_char **data = malloc(height * sizeof(u_char *));
    for (int i = 0; i < height; i++) {
        data[i] = malloc(width * sizeof(u_char));
    }

    output_image = malloc(sizeof(PGM));
    output_image->width = width;
    output_image->height = height;
    output_image->data = data;

    pthread_t threads[num_threads];
    Lines lines[num_threads];

    int size = (int) floor((double) height / num_threads);
    for (int i = 0; i < num_threads - 1; i++) {
        lines[i].from = i * size + 1;
        lines[i].to = lines[i].from + size - 1;
    }
    lines[num_threads - 1].from = (num_threads - 1) * size + 1;
    lines[num_threads - 1].to = height;

    int s;
    u_char max = 0;
    void *res_ptr;
    u_char res;

    struct timeval start;
    if (gettimeofday(&start, NULL) == -1) error(EXIT_FAILURE, errno, "gettimeofday");

    for (int i = 0; i < num_threads; i++) {
        s = pthread_create(&threads[i], NULL, thread_start, &lines[i]);
        if (s != 0) error(EXIT_FAILURE, s, "pthread_create");
    }

    for (int i = 0; i < num_threads; i++) {
        s = pthread_join(threads[i], &res_ptr);
        if (s != 0) error(EXIT_FAILURE, s, "pthread_join");

        res = *(u_char *) res_ptr;
        if (res > max) max = res;
        free(res_ptr);
    }

    struct timeval end;
    if (gettimeofday(&end, NULL) == -1) error(EXIT_FAILURE, errno, "gettimeofday");

    __suseconds_t us = end.tv_usec - start.tv_usec;
    __time_t _s = end.tv_sec - start.tv_sec;
    if (us < 0) {
        _s -= 1;
        us += 1000000;
    }

    end.tv_sec = _s;
    end.tv_usec = us;

    printf("threads: %d, elapsed time: %3ld.%.6ld\n", num_threads, end.tv_sec, end.tv_usec);

    output_image->max = max;

    return output_image;
}

char *parse_str(const char *file) {
    int fd = open(file, O_RDONLY);
    if (fd == -1) error(EXIT_FAILURE, errno, "open");

    char buffer[BUFFER_SIZE];
    size_t RESULT_SIZE = 1024 * BUFFER_SIZE;
    size_t result_size = RESULT_SIZE;
    char *result = malloc(sizeof(char) * result_size);
    ssize_t bytes_read;
    ssize_t bytes_written = 0;

    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
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

PGM *parse_image(char *image_str) {
    char *ptr;
    char *key = strtok_r(image_str, " \n\t", &ptr);

    if (strcmp(key, "P2") != 0) invalid_file();

    int width = (int) strtoul(ptr, &ptr, 10);
    int height = (int) strtoul(ptr, &ptr, 10);

    ulong _max = strtoul(ptr, &ptr, 10);
    u_char max = 0;
    if (_max > MAX_GRAY_VALUE) invalid_file();
    else max = (u_char) _max;

    u_char **data = malloc(height * sizeof(u_char *));

    for (int i = 0; i < height; i++) {
        data[i] = malloc(width * sizeof(u_char));
    }

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (ptr != NULL) {
                data[i][j] = (u_char) strtoul(ptr, &ptr, 10);
            } else invalid_file();
        }
    }

    PGM *result = malloc(sizeof(PGM));
    result->width = width;
    result->height = height;
    result->max = max;
    result->data = data;

    return result;
}

Filter *parse_filter(char *filter) {
    char *ptr;
    int c = (int) strtoul(filter, &ptr, 10);

    double **data = malloc(c * sizeof(double *));
    for (int i = 0; i < c; i++) {
        data[i] = malloc(c * sizeof(double));
    }

    for (int i = 0; i < c; i++) {
        for (int j = 0; j < c; j++) {
            if (ptr != NULL) {
                data[i][j] = strtod(ptr, &ptr);
            } else invalid_file();
        }
    }

    Filter *result = malloc(sizeof(Filter));
    result->size = c;
    result->data = data;

    return result;
}

void export_image(PGM *image, const char *file) {
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) error(EXIT_FAILURE, errno, "open");

    char buffer[BUFFER_SIZE];
    size_t s = (size_t) sprintf(buffer, "P2\n%d %d\n%d\n", image->width, image->height, image->max);

    int l = 0;

    for (int i = 0; i < image->height; i++) {
        for (int j = 0; j < image->width; j++) {
            if (s + 5 > BUFFER_SIZE) {
                if (write(fd, buffer, s) == -1) error(EXIT_FAILURE, errno, "write");
                s = 0;
            }

            l++;
            s += sprintf(&buffer[s], "%3d ", image->data[i][j]);
            if (l == 17) {
                buffer[s++] = '\n';
                l = 0;
            }
        }
    }

    if (write(fd, buffer, s) == -1) error(EXIT_FAILURE, errno, "write");

    close(fd);
}

void print_usage_and_exit() {
    printf("usage: <program> threads input filter output\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 5) print_usage_and_exit();

    int threads = atoi(argv[1]);
    char *input_file = argv[2];
    char *filter_file = argv[3];
    char *output_file = argv[4];

    char *image_str = parse_str(input_file);
    PGM *image = parse_image(image_str);

    char *filter_str = parse_str(filter_file);
    Filter *filter = parse_filter(filter_str);

    PGM *result = filter_image(image, filter, threads);
    export_image(result, output_file);

    pgm_free(image);
    pgm_free(result);
    filter_free(filter);

    free(image_str);
    free(filter_str);

    return 0;
}
