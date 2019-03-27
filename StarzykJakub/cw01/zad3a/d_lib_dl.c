#include "../zad1/d_lib.h"

#include <dlfcn.h>
#include <stdio.h>

static int (*__d_add_block)(DCharArray *, size_t);
static int (* __d_delete_block)(DCharArray *, size_t);
static DCharArray *(*__d_create_dynamic_array)(size_t, size_t);
static DCharArray *(*__d_create_static_array)(size_t, size_t , void *, size_t);
static int (*__d_delete_array)(DCharArray *);
static CHAR *(*__d_closest_match)(DCharArray *, size_t);
static CHAR *(*__d_block_address)(DCharArray *, size_t);

int d_add_block(DCharArray *d_array, size_t index) {
    return (*__d_add_block)(d_array, index);
}

int d_delete_block(DCharArray *d_array, size_t index) {
    return (*__d_delete_block)(d_array, index);
}

DCharArray *d_create_dynamic_array(size_t array_size, size_t block_size) {
    return (*__d_create_dynamic_array)(array_size, block_size);
}

DCharArray *d_create_static_array(size_t array_size, size_t block_size, void *buffer, size_t buffer_size) {
    return (*__d_create_static_array)(array_size, block_size, buffer, buffer_size);
}

int d_delete_array(DCharArray *d_array) {
    return (*__d_delete_array)(d_array);
}

CHAR *d_closest_match(DCharArray *d_array, size_t index) {
    return (*__d_closest_match)(d_array, index);
}

CHAR *d_block_address(DCharArray *d_array, size_t index) {
    return (*__d_block_address)(d_array, index);
}

void load_lib() {
    char *lib_name = "libex1.so";
    void *handle = dlopen(lib_name, RTLD_LAZY);

    if (handle == NULL) {
        perror("Cannot load library libex1.so");
        exit(1);
    }

    __d_add_block = dlsym(handle, "d_add_block");
    __d_delete_block = dlsym(handle, "d_delete_block");
    __d_create_dynamic_array = dlsym(handle, "d_create_dynamic_array");
    __d_create_static_array = dlsym(handle, "d_create_static_array");
    __d_delete_array = dlsym(handle, "d_delete_array");
    __d_closest_match = dlsym(handle, "d_closest_match");
    __d_block_address = dlsym(handle, "d_block_address");

}
