#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

#include "../zad1/d_lib.h"

#ifdef DYNAMIC_LOAD
#include "../zad3a/d_lib_dl.h"
#endif

#define INV_NO_OF_ARGS -3

#define BUFFER_SIZE_CHARS 5000000
#define BUFFER_SIZE_BYTES (BUFFER_SIZE_CHARS * sizeof ( CHAR ))

CHAR *static_buffer_ptr;
static char charset[95];

struct BenchmarkSpecification {
    char *benchmark_name;
    size_t size;
    size_t block_size;
    bool allocate_static;
    DCharArray *d_array;
    size_t block_index;
    size_t num_blocks_to_test;
};
typedef struct BenchmarkSpecification BenchmarkSpecification;

struct BenchmarkResult {
    struct timeval real;
    struct timeval user;
    struct timeval sys;
};
typedef struct BenchmarkResult BenchmarkResult;

typedef void (*BenchmarkFunction)(BenchmarkSpecification *);
typedef void (*InitFunction)(BenchmarkSpecification *);

int _invalid_number_of_ops() {
    perror("Invalid number of operations.");
    return INV_NO_OF_ARGS;
}

int _invalid_test_number() {
    perror("Invalid test number.");
    return INVALID_ARGUMENT;
}

static void init_charset() {
    int i = 0;

    for (int c = 32; c < 127; c++) {
        charset[i++] = (char) c;
    }
}

void util_rand_string(CHAR *str, size_t size) {
    for (size_t n = 0; n < size; n++) {
        int key = rand() % (int)(sizeof(charset) - 1);
        str[n] = charset[key];
    }

    str[size] = '\0';
}


void create_array_with_sample_data(BenchmarkSpecification *spec) {
    DCharArray *array = d_create_dynamic_array(spec->size, spec->block_size);

    if (array != NULL) {
        for (size_t i = 0; i < array->size; i++) {
            int res = d_add_block(array, i);

            if (res != OK) {
                exit(EXIT_FAILURE);
            }

            util_rand_string(d_block_address(array, i), array->block_size);
        }

        spec->d_array = array;
    } else {
        exit(EXIT_FAILURE);
    }
}

void create_array(BenchmarkSpecification *spec) {
    DCharArray *array;

    if (spec->allocate_static) {
        array = d_create_static_array(spec->size, spec->block_size, static_buffer_ptr, BUFFER_SIZE_BYTES);
    } else {
        array = d_create_dynamic_array(spec->size, spec->block_size);
    }

    if (array != NULL) {
        spec->d_array = array;
    } else {
        exit(EXIT_FAILURE);
    }
}

void op_create_array(BenchmarkSpecification *spec) {
    create_array(spec);
}

void op_closest_match(BenchmarkSpecification *spec) {
    d_closest_match(spec->d_array, spec->block_index);
}

void op_sequential_replace(BenchmarkSpecification *spec) {
    for (size_t i = 0; i < spec->num_blocks_to_test; i++) {
        d_delete_block(spec->d_array, i);
    }

    for (size_t i = 0; i < spec->num_blocks_to_test; i++) {
        d_add_block(spec->d_array, i);
    }
}

void op_alternately_replace(BenchmarkSpecification *spec) {
    for (size_t i = 0; i < spec->num_blocks_to_test; i++) {
        d_add_block(spec->d_array, i);
    }
}


long diff_time_val(struct timeval start, struct timeval end) {
    long start_micros = start.tv_usec + start.tv_sec * 1000000;
    long end_micros = end.tv_usec + end.tv_sec * 1000000;
    return end_micros - start_micros;
}

void benchmark(InitFunction init_fun, BenchmarkFunction bench_fun, BenchmarkSpecification *spec, const int cache_loops, const int test_loops) {
    if (init_fun != NULL) {
        init_fun(spec);
    }

    BenchmarkResult start;
    BenchmarkResult end;
    struct rusage usage;

    for (int i = 0; i < cache_loops; i++) {
        bench_fun(spec);
    }

    getrusage(RUSAGE_SELF, &usage);
    gettimeofday(&start.real, NULL);
    start.user = usage.ru_utime;
    start.sys = usage.ru_stime;

    for (int i = 0; i < test_loops; i++) {
        bench_fun(spec);
    }

    getrusage(RUSAGE_SELF, &usage);
    gettimeofday(&end.real, NULL);
    end.user = usage.ru_utime;
    end.sys = usage.ru_stime;


    double real, sys, user;
    real = diff_time_val(start.real, end.real) / (double) test_loops * 1000.0;
    sys = diff_time_val(start.sys, end.sys) / (double) test_loops * 1000.0;
    user = diff_time_val(start.user, end.user) / (double) test_loops * 1000.0;

    printf("Benchmark for: %s\n", spec->benchmark_name);
    printf("------------------------------------\n");
    printf("Array length: %zu\n", spec->size);
    printf("Block length: %zu\n", spec->block_size);
    printf("Static allocation: %s\n", spec->allocate_static ? "true" : "false");
    printf("------------------------------------\n");
    printf("User CPU time: %f ns\n", user);
    printf("System CPU time: %f ns\n", sys);
    printf("Real time: %f ns\n\n", real);

    fprintf(stderr, "%s,%.3f,%.3f,%.3f\n", spec->benchmark_name, user, sys, real);

    if (strcmp(spec->benchmark_name, "closest_match") == 0) {
        printf("Closest match to\n%s\nis\n%s\n\n",
               d_block_address(spec->d_array, spec->block_index),
               d_closest_match(spec->d_array, spec->block_index));
    }

}


int util_parse_number(char *s_number, size_t low, size_t high) {
    int number = atoi(s_number);

    if (number >= low && number <= high) {
        return number;
    } else {
        exit(EXIT_FAILURE);
    }
}


BenchmarkSpecification *parse_args(int num_ops, int num_r_args, char *argv[]) {
    BenchmarkSpecification *result = calloc(num_ops, sizeof(BenchmarkSpecification));
    BenchmarkSpecification util_spec;
    util_spec.size = util_parse_number(argv[1], 1, __SIZE_MAX__);
    util_spec.block_size = util_parse_number(argv[2], 1, __SIZE_MAX__);

    if ((argv[3][1]) == 'd') {
        util_spec.allocate_static = false;
    } else {
        util_spec.allocate_static = true;
    }

    for (size_t i = 1; i <= num_ops; i++) {
        BenchmarkSpecification *spec = &result[i - 1];
        spec->allocate_static = util_spec.allocate_static;
        spec->size = util_spec.size;
        spec->block_size = util_spec.block_size;
        int number;

        switch (argv[num_r_args + i][0]) {
        case 'c':
            spec->benchmark_name = "create_array";
            break;

        case 'm':
            spec->benchmark_name = "closest_match";
            number = util_parse_number(&argv[num_r_args + i][1], 0, util_spec.size - 1);
            spec->block_index = number;
            break;

        case 's':
            spec->benchmark_name = "sequential_replace";
            number = util_parse_number(&argv[num_r_args + i][1], 1, util_spec.size);
            spec->num_blocks_to_test = number;
            break;

        case 'a':
            spec->benchmark_name = "alternately_replace";
            number = util_parse_number(&argv[num_r_args + i][1], 1, util_spec.size);
            spec->num_blocks_to_test = number;
            break;

        default:
            perror("Unrecognized operation.");
            return NULL;
        }

    }

    return result;
}



int main(int argc, char *argv[]) {
#ifdef DYNAMIC_LOAD
    load_lib();
#endif

    srand(time(0));
    init_charset();

    CHAR buffer[BUFFER_SIZE_CHARS];
    static_buffer_ptr = buffer;

    int num_r_args = 3;
    int num_ops = argc - num_r_args - 1;

    BenchmarkSpecification *specs = parse_args(num_ops, num_r_args, argv);

    if (specs == NULL) {
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_ops; i++) {
        BenchmarkSpecification *spec = &specs[i];
        InitFunction init_fun = NULL;
        BenchmarkFunction bench_fun = NULL;

        if (strcmp(spec->benchmark_name, "create_array") == 0) {
            init_fun = NULL;
            bench_fun = &op_create_array;
        } else if (strcmp(spec->benchmark_name, "closest_match") == 0) {
            init_fun = &create_array_with_sample_data;
            bench_fun = &op_closest_match;
        } else if (strcmp(spec->benchmark_name, "sequential_replace") == 0) {
            init_fun = &create_array;
            bench_fun = &op_sequential_replace;
        } else if (strcmp(spec->benchmark_name, "alternately_replace") == 0) {
            init_fun = &create_array;
            bench_fun = &op_alternately_replace;
        }

        benchmark(init_fun, bench_fun, spec, 1000, 10000);
    }

    free(specs);

    return 0;
}
