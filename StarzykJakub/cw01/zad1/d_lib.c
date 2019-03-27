#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>
#include <limits.h>

#include "d_lib.h"
#include "d_lib_internal.h"


int d_add_block(DCharArray *d_array, size_t index) {
    if (! _d_is_valid_index(d_array, index)) {
        return _d_error_invalid_index_argument();
    }

    _d_free_block_if_needed(d_array, index);

    if (d_array->dynamic) {
        CHAR *new_char_array = calloc(d_array->block_size, sizeof(CHAR));

        if (new_char_array == NULL) {
            return _d_error_memory_allocation();
        } else {
            CHAR **blocks = (CHAR **)d_array->block;
            blocks[index] = new_char_array;
            return OK;
        }
    } else {
        CHAR *addr = _d_block_address(d_array, index);
        void *block_buffer = addr - sizeof(bool);
        *((bool *)block_buffer) = true;
        return OK;
    }
}


int d_delete_block(DCharArray *d_array, size_t index) {
    if (_d_is_valid_index(d_array, index)) {
        _d_free_block_if_needed(d_array, index);
        return OK;
    } else {
        return _d_error_invalid_index_argument();
    }
}


DCharArray *d_create_dynamic_array(size_t array_size, size_t block_size) {
    DCharArray *result = malloc(sizeof(DCharArray));

    if (result == NULL) {
        _d_error_memory_allocation();
        return NULL;
    }

    CHAR **new_char_p_array = calloc(array_size, sizeof(CHAR *));

    if (new_char_p_array == NULL) {
        _d_error_memory_allocation();
        free(result);
        return NULL;
    } else {
        result->size = array_size;
        result->block_size = block_size;
        result->block = new_char_p_array;
        result->dynamic = true;
        return result;
    }
}


DCharArray *d_create_static_array(size_t array_size, size_t block_size, void *buffer, size_t buffer_size) {
    DCharArray *result = malloc(sizeof(DCharArray));

    if (result == NULL) {
        _d_error_memory_allocation();
        return NULL;
    }

    if (_d_buffer_size(array_size, block_size) > buffer_size) {
        _d_error_memory_allocation();
        free(result);
        return NULL;
    }

    result->size = array_size;
    result->block_size = block_size;
    result->block = buffer;
    result->dynamic = false;
    return result;
}


int d_delete_array(DCharArray *d_array) {
    if (d_array == NULL) {
        return INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < d_array->size; i++) {
        _d_free_block_if_needed(d_array, i);
    }

    if (d_array->dynamic) {
        free((CHAR **)d_array->block);
    }

    free(d_array);
    return OK;
}


CHAR *d_closest_match(DCharArray *d_array, size_t index) {
    if (!_d_is_valid_array(d_array)) {
        return NULL;
    }

    if (!_d_is_valid_index(d_array, index)) {
        _d_error_invalid_index_argument();
        return NULL;
    }

    if (_d_block_address(d_array, index) == NULL) {
        _d_error_invalid_index_argument();
        return NULL;
    }

    int array_sum = _char_block_sum(_d_block_address(d_array, index), d_array->block_size);
    int min_diff = INT_MAX;
    int d_sum, diff;
    CHAR *d_element;
    CHAR *result = NULL;

    for (size_t i = 0; i < d_array->size; i++) {
        if (i == index) {
            continue;
        }

        d_element = _d_block_address(d_array, i);

        if (d_element != NULL) {
            d_sum = _char_block_sum(d_element, d_array->block_size);
            diff = abs(d_sum - array_sum);

            if (diff < min_diff) {
                min_diff = diff;
                result = d_element;
            }
        }
    }

    return result;
}


CHAR *d_block_address(DCharArray *d_array, size_t index) {
    if (_d_is_valid_array(d_array)) {
        return _d_block_address(d_array, index);
    } else {
        return NULL;
    }
}
