#include "d_lib_internal.h"

#include <stdio.h>
#include <string.h>


int _d_error_invalid_index_argument() {
    perror("Invalid index.");
    return INVALID_ARGUMENT;
}

int _d_error_memory_allocation() {
    perror("Failed to allocate memory.");
    return ALLOCATION_ERROR;
}

bool _d_is_valid_array(DCharArray *d_array) {
    if (d_array == NULL || d_array->size == 0 || d_array->block_size == 0) {
        return false;
    } else {
        return true;
    }
}

void _d_free_block_if_needed(DCharArray *d_array, size_t index) {
    CHAR *addr = _d_block_address(d_array, index);

    if (d_array-> dynamic) {
        if (addr != NULL) {
            free(addr);
            ((CHAR **)d_array->block)[index] = NULL;
        }
    } else {
        void *block_buffer = addr - sizeof(bool);
        bool used = *((bool *)block_buffer);

        if (used) {
            size_t block_size = sizeof(bool) + d_array->block_size * sizeof(CHAR);
            memset(block_buffer, 0, block_size);
        }
    }
}

int _d_is_valid_index(DCharArray *d_array, size_t index) {
    return d_array != NULL && index <= d_array->size;
}

int _char_block_sum(CHAR *block, size_t block_size) {
    int sum = 0;

    for (size_t c = 0; c < block_size; c++) {
        sum += block[c];
    }

    return sum;
}

CHAR *_d_block_address(DCharArray *d_array, size_t index) {
    if (_d_is_valid_index(d_array, index)) {
        if (d_array->dynamic) {
            return ((CHAR **)d_array->block)[index];
        } else {
            size_t offset = index * (d_array->block_size + sizeof(bool)) + sizeof(bool);
            return d_array->block + offset;
        }
    } else {
        _d_error_invalid_index_argument();
        return NULL;
    }
}

size_t _d_buffer_size(size_t array_size, size_t block_size) {
    return array_size * (sizeof(bool) + block_size * sizeof(CHAR));
}
