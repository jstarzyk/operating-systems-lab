#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef unsigned char CHAR;

struct DCharArray {
    size_t size;
    size_t block_size;
    bool dynamic;

    // if (dynamic)
    //    CHAR **block;
    // else
    //    typedef struct {
    //        bool block_present;
    //        CHAR[block_size] data;
    //    } BLOCK;
    //    BLOCK[] block;
    void *block;
};

typedef struct DCharArray DCharArray;

#define OK 0
#define INVALID_ARGUMENT -1
#define ALLOCATION_ERROR -2

int d_add_block(DCharArray *d_array, size_t index);
int d_delete_block(DCharArray *d_array, size_t index);
DCharArray *d_create_dynamic_array(size_t array_size, size_t block_size);
DCharArray *d_create_static_array(size_t array_size, size_t block_size, void *buffer, size_t buffer_size);
int d_delete_array(DCharArray *d_array);
CHAR *d_closest_match(DCharArray *d_array, size_t index);
CHAR *d_block_address(DCharArray *d_array, size_t index);
