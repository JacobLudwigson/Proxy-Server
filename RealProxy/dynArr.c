#include "dynArr.h"
// Initialize the array
void da_init(DynamicArray *arr) {
    arr->items = malloc(sizeof(void*) * INITIAL_CAPACITY);
    arr->size = 0;
    arr->capacity = INITIAL_CAPACITY;
}

// Free the array
void da_free(DynamicArray *arr) {
    free(arr->items);
    arr->items = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

// Resize helper
void da_resize(DynamicArray *arr, size_t new_capacity) {
    arr->items = realloc(arr->items, sizeof(void*) * new_capacity);
    arr->capacity = new_capacity;
}

// Add an item
void da_push(DynamicArray *arr, void *item) {
    if (arr->size == arr->capacity) {
        da_resize(arr, arr->capacity * 2);
    }
    arr->items[arr->size++] = item;
}

// Remove at index (shifts remaining)
void da_remove(DynamicArray *arr, size_t index) {
    if (index >= arr->size) return;
    for (size_t i = index; i < arr->size - 1; i++) {
        arr->items[i] = arr->items[i + 1];
    }
    arr->size--;
}
