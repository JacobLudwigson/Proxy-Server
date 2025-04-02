#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stdlib.h>
typedef struct {
    void **items;
    size_t size;
    size_t capacity;
} DynamicArray;
#define INITIAL_CAPACITY 8

void da_init(DynamicArray *arr);
void da_free(DynamicArray *arr);
void da_push(DynamicArray *arr, void *item);
void da_remove(DynamicArray *arr, size_t index);

#endif