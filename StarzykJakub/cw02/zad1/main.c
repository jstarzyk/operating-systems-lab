#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>


static const char generate_string[] = "generate";
static const char sort_string[] = "sort";
static const char copy_string[] = "copy";
static const unsigned char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";


void _create_rand_string(unsigned char *str, size_t size) {
    for (size_t n = 0; n < size; n++) {
        int key = rand() % (int)(sizeof(charset) - 1);
        str[n] = charset[key];
    }

    str[size] = '\0';
}

struct Params {
    char *action_name;
    char *file;
    char *file_from;
    char *file_to;
    size_t number_of_records;
    size_t record_size;
    size_t buffer_size;
    bool use_sys;
};
typedef struct Params Params;

struct TimevalResult {
    struct timeval real;
    struct timeval user;
    struct timeval sys;
};
typedef struct TimevalResult TimevalResult;

typedef void (*IOAction)(Params *);

struct MicroResult {
    long real;
    long user;
    long sys;
};
typedef struct MicroResult MicroResult;


void _argument_and_function_dont_match() {
    fprintf(stderr, "invalid action specified in parameters\n");
    exit(EXIT_FAILURE);
}

void _seek_error() {
    fprintf(stderr, "seek error\n");
    exit(EXIT_FAILURE);
}

void _close(bool sys, int fd, FILE *pt) {
    if (sys) {
        close(fd);
    } else {
        fclose(pt);
    }
}

long _seek_to_index(bool sys, size_t record_index, size_t record_size, int fd, FILE *pt) {
    long seek_result;

    if (sys) {
        seek_result = lseek(fd, record_index * record_size, SEEK_SET);
    } else {
        seek_result = fseek(pt, record_index * record_size, SEEK_SET);
        if (seek_result != -1) {
            seek_result = ftell(pt);
        }
    }

    if (seek_result == -1) {
        _seek_error();
    }

    return seek_result;
}

long _seek_from_current(bool sys, ssize_t n, size_t record_size, int fd, FILE *pt) {
    long seek_result;

    if (sys) {
        seek_result = lseek(fd, n * record_size, SEEK_CUR);
    } else {
        seek_result = fseek(pt, n * record_size, SEEK_CUR);
        if (seek_result != -1) {
            seek_result = ftell(pt);
        }
    }

    if (seek_result == -1) {
        _seek_error();
    }

    return seek_result;
}

void _read_or_write_error(bool f_read, const size_t nbytes, const char *file) {
    if (f_read) {
        fprintf(stderr, "could not read %zu bytes from %s\n", nbytes, file);
    } else {
        fprintf(stderr, "could not write %zu bytes to %s\n", nbytes, file);
    }

    exit(EXIT_FAILURE);
}

void _read_or_write(bool f_read, bool f_sys, unsigned char *buffer, size_t buffer_size, int fd, FILE *pt, char *file) {
    ssize_t bytes_processed = -1;
    
    if (f_read) {
        if (f_sys) {
            bytes_processed = read(fd, buffer, buffer_size);
        } else {
            fread(buffer, buffer_size, 1, pt);
        }
    } else {
        if (f_sys) {
            bytes_processed = write(fd, buffer, buffer_size);
        } else {
            fwrite(buffer, buffer_size, 1, pt);
        }
    }

    if (bytes_processed == -1 && ferror(pt) != 0) {
        _close(f_sys, fd, pt);
        _read_or_write_error(f_read, buffer_size, file);
    }

    _seek_from_current(f_sys, -1, buffer_size, fd, pt);
}




void copy(Params *params) {
    if (strcmp(params->action_name, copy_string) != 0) {
        _argument_and_function_dont_match();
    }

    int fd_from = -1;
    FILE *pt_from = NULL;

    if (params->use_sys) {
        fd_from = open(params->file_from, O_RDONLY);
    } else {
        pt_from = fopen(params->file_from, "r");
    }

    if (fd_from < 0 && pt_from == NULL) {
        fprintf(stderr, "could not open %s for reading\n", params->file_from);
        exit(EXIT_FAILURE);
    }

    int fd_to = -1;
    FILE *pt_to = NULL;

    if (params->use_sys) {
        fd_to = open(params->file_to, O_WRONLY | O_EXCL | O_APPEND | O_CREAT, 0644);
    } else {
        pt_to = fopen(params->file_to, "ax");
    }

    if (fd_to < 0 && pt_to == NULL) {
        fprintf(stderr, "could not create %s\n", params->file_to);
        exit(EXIT_FAILURE);
    }

    unsigned char buffer[params->buffer_size];
    ssize_t bytes_read = -1;
    ssize_t bytes_written = -1;

    for (size_t r = 0; r < params->number_of_records; r++) {
        if (params->use_sys) {
            bytes_read = read(fd_from, buffer, params->buffer_size);
        } else {
            fread(buffer, 1, params->buffer_size, pt_from);
        }

        if (bytes_read == -1 && ferror(pt_from) != 0) {
            _close(params->use_sys, fd_from, pt_from);
            _close(params->use_sys, fd_to, pt_to);
            fprintf(stderr, "could not read %zu bytes from %s\n", params->number_of_records * params->buffer_size, params->file_from);
            fprintf(stderr, "%zu bytes copied from %s to %s\n", r * params->buffer_size, params->file_from, params->file_to);
            exit(EXIT_SUCCESS);
        }
        
        if (params->use_sys) {
            bytes_written = write(fd_to, buffer, params->buffer_size);
        } else {
            fwrite(buffer, 1, params->buffer_size, pt_to);
        }

        if (bytes_written == -1 && ferror(pt_to) != 0) {
            _close(params->use_sys, fd_from, pt_from);
            _close(params->use_sys, fd_to, pt_to);
            _read_or_write_error(0, params->buffer_size, params->file_to);
        }
    }

    _close(params->use_sys, fd_from, pt_from);
    _close(params->use_sys, fd_to, pt_to);
}

void generate(Params *params) {
    if (strcmp(params->action_name, generate_string) != 0) {
        _argument_and_function_dont_match();
    }

    int fd_file = open(params->file, O_WRONLY | O_EXCL | O_APPEND | O_CREAT, 0644);

    if (fd_file == -1) {
        fprintf(stderr, "could not create %s\n", params->file);
        exit(EXIT_FAILURE);
    } else {
        printf("%s created", params->file);
    }

    const size_t nbytes = params->record_size;// = params->number_of_records * params->record_size;
    unsigned char buffer[nbytes + 1];
    ssize_t bytes_written;
    
    for (size_t i = 0; i < params->number_of_records; i++) {
        _create_rand_string(buffer, nbytes);
        bytes_written = write(fd_file, buffer, nbytes);
        
        if (bytes_written == -1) {
            close(fd_file);
            _read_or_write_error(0, nbytes, params->file);
        }
    }

}



long _get_file_size(bool sys, int fd, FILE *pt) {
    long seek_result;
    long file_size;

    if (sys) {
        seek_result = lseek(fd, 0, SEEK_END);

        if (seek_result < 0) {
            _seek_error();
        }

        file_size = seek_result;

        if (lseek(fd, 0, SEEK_SET) < 0) {
            _seek_error();
        }
    } else {
        if (fseek(pt, 0, SEEK_END) < 0) {
            _seek_error();
        }

        seek_result = ftell(pt);

        if (seek_result < 0) {
            _seek_error();
        }

        file_size = seek_result;
        rewind(pt);
    }

    return file_size;
}



unsigned char _get_key(bool sys, int fd, FILE *pt) {
    unsigned char key[1];
    ssize_t read_result = -1;

    if (sys) {
        read_result = read(fd, key, 1);
    } else {
        fread(key, 1, 1, pt);
    }

    if (read_result == -1 && ferror(pt) != 0) {
        _close(sys, fd, pt);
        fprintf(stderr, "could not read key");
        exit(EXIT_FAILURE);
    }

    _seek_from_current(sys, -1, 1, fd, pt);
    return key[0];
}




void sort(Params *params) {
    if (strcmp(params->action_name, sort_string) != 0) {
        _argument_and_function_dont_match();
    }

    int fd = -1;
    FILE *pt = NULL;

    bool use_sys = params->use_sys;
    
    if (use_sys) {
        fd = open(params->file, O_RDWR);
    } else {
        pt = fopen(params->file, "r+");
    }
    
    size_t buffer_size = params->record_size;

    long file_size = _get_file_size(use_sys, fd, pt);
    size_t sort_size = params->number_of_records * buffer_size;

    if (file_size < sort_size) {
        fprintf(stderr, "%s does not contain %zu bytes for sorting\n", params->file, sort_size);
        exit(EXIT_FAILURE);
    } else if (file_size > sort_size) {
        fprintf(stderr, "sorting %zu from %zu bytes...\n", sort_size, file_size);
    }

    unsigned char key_buffer[buffer_size + 1];
    key_buffer[buffer_size] = '\0';
    unsigned char cmp_buffer[buffer_size + 1];
    cmp_buffer[buffer_size] = '\0';

    for (size_t i = 1; i < params->number_of_records; i++) {
        long offset_start_iter = _seek_to_index(use_sys, i, buffer_size, fd, pt);
        
//         unsigned char key = _get_key(use_sys, fd, pt);
//         _seek_from_current(use_sys, -1, buffer_size, fd, pt);
//         unsigned char cmp = _get_key(use_sys, fd, pt);
//         
//         if (cmp <= key) {
//             continue;
//         }
//         
//         _seek_from_current(use_sys, 1, buffer_size, fd, pt);

        _read_or_write(true, use_sys, key_buffer, buffer_size, fd, pt, params->file);

        _seek_from_current(use_sys, -1, buffer_size, fd, pt);
        bool reached_start = false;

        while (true) {
            _read_or_write(true, use_sys, cmp_buffer, buffer_size, fd, pt, params->file);

            if (cmp_buffer[0] <= key_buffer[0]) {
                break;
            }

            _seek_from_current(use_sys, 1, buffer_size, fd, pt);
            
            _read_or_write(false, use_sys, cmp_buffer, buffer_size, fd, pt, params->file);

            if (_seek_from_current(use_sys, -1, buffer_size, fd, pt) > 0) {
                _seek_from_current(use_sys, -1, buffer_size, fd, pt);
            } else {
                reached_start = true;
                break;
            }
        }

        long offset_end_iter = 0;

        if (!reached_start) {
            offset_end_iter = _seek_from_current(use_sys, 1, buffer_size, fd, pt);
        }

        if (offset_start_iter == offset_end_iter) {
            continue;
        }

        _read_or_write(false, use_sys, key_buffer, buffer_size, fd, pt, params->file);
    }

    _close(use_sys, fd, pt);
}


long diff_time_val(struct timeval start, struct timeval end) {
    long start_micros = start.tv_usec + start.tv_sec * 1000000;
    long end_micros = end.tv_usec + end.tv_sec * 1000000;
    return end_micros - start_micros;
}

MicroResult run_benchmark(IOAction function, Params *params) {
    TimevalResult start;
    TimevalResult end;
    struct rusage usage;


    getrusage(RUSAGE_SELF, &usage);
    gettimeofday(&start.real, NULL);
    start.user = usage.ru_utime;
    start.sys = usage.ru_stime;

    function(params);
    
    getrusage(RUSAGE_SELF, &usage);
    gettimeofday(&end.real, NULL);
    end.user = usage.ru_utime;
    end.sys = usage.ru_stime;

    MicroResult result;
//     double real, sys, user;
    result.real = diff_time_val(start.real, end.real);
    result.sys = diff_time_val(start.sys, end.sys);
    result.user = diff_time_val(start.user, end.user);

//     printf("U: %.3f s\n", user);
//     printf("S: %.3f s\n", sys);
//     printf("R: %.3f s\n\n", real);
    
    return result;
}





void _parse_implementation_variant(Params *parsed, char *str) {
    if (strcmp(str, "sys") == 0) {
        parsed->use_sys = true;
    } else if (strcmp(str, "lib") == 0) {
        parsed->use_sys = false;
    } else {
        fprintf(stderr, "implementation variant not specified\n");
        exit(EXIT_FAILURE);
    }
}

void _parse_data_args(Params *parsed, char *argv[]) {
    size_t number_of_records = atoi(argv[0]);
    size_t record_size = atoi(argv[1]);

    if (number_of_records > 0 && record_size > 0) {
        parsed->number_of_records = number_of_records;

        if (strcmp(parsed->action_name, copy_string) == 0) {
            parsed->buffer_size = record_size;
        } else {
            parsed->record_size = record_size;
        }
    } else {
        fprintf(stderr, "invalid data arguments\n");
        exit(EXIT_FAILURE);
    }
}

void _assert_number_of_required(const char *op_name, int argc, int required) {
    if (argc != required) {
        fprintf(stderr, "%s requires %d arguments\n", op_name, required - 2);
        exit(EXIT_FAILURE);
    }
}

Params *parse_args(int argc, char *argv[]) {
    if (argc == 1) {
        exit(EXIT_FAILURE);
    }

    Params *parsed = calloc(1, sizeof(Params));
    parsed->action_name = argv[1];

    if (strcmp(parsed->action_name, generate_string) == 0) {
        _assert_number_of_required(generate_string, argc, 5);
        parsed->file = argv[2];
        _parse_data_args(parsed, &argv[3]);
    } else if (strcmp(parsed->action_name, sort_string) == 0) {
        _assert_number_of_required(sort_string, argc, 6);
        parsed->file = argv[2];
        _parse_data_args(parsed, &argv[3]);
        _parse_implementation_variant(parsed, argv[5]);
    } else if (strcmp(parsed->action_name, copy_string) == 0) {
        _assert_number_of_required(copy_string, argc, 7);
        parsed->file_from = argv[2];
        parsed->file_to = argv[3];
        _parse_data_args(parsed, &argv[4]);
        _parse_implementation_variant(parsed, argv[6]);
    } else {
        free(parsed);
        fprintf(stderr, "invalid operation\n");
        exit(EXIT_FAILURE);
    }

    return parsed;
}


void _print_results() {
    
}

void _test_print_params(Params *params) {
    printf("action_name: %s\n", params->action_name);
    printf("file: %s\n", params->file);
    printf("file_from: %s\n", params->file_from);
    printf("file_to: %s\n", params->file_to);
    printf("number_of_records: %zu\n", params->number_of_records);
    printf("record_size: %zu\n", params->record_size);
    printf("buffer_size: %zu\n", params->buffer_size);
    printf("use_sys: %d\n", params->use_sys);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    Params *parsed = parse_args(argc, argv);
    MicroResult result;

    if (strcmp(parsed->action_name, generate_string) == 0) {
        generate(parsed);
    } else if (strcmp(parsed->action_name, sort_string) == 0) {
        result = run_benchmark(&sort, parsed);
        const double s_in_micros = 1000000.0;
        printf("U: %.3f s\n", result.user / s_in_micros);
        printf("S: %.3f s\n", result.sys / s_in_micros);
        printf("R: %.3f s\n\n", result.real / s_in_micros);
    } else if (strcmp(parsed->action_name, copy_string) == 0) {
        result = run_benchmark(&copy, parsed);
        printf("U: %ld us\n", result.user);
        printf("S: %ld us\n", result.sys);
        printf("R: %ld us\n\n", result.real);
    }

    
    
    exit(EXIT_SUCCESS);
}
