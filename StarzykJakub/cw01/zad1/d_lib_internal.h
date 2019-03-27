#pragma once

#include "d_lib.h"

bool _d_is_valid_array(DCharArray *d_array);
void _d_free_block_if_needed(DCharArray *d_array, size_t index);
int _d_is_valid_index(DCharArray *d_array, size_t index);
int _d_error_invalid_index_argument();
int _d_error_memory_allocation();
int _char_block_sum(CHAR *block, size_t block_size);
CHAR *_d_block_address(DCharArray *d_array, size_t index);
size_t _d_buffer_size(size_t array_size, size_t block_size);
