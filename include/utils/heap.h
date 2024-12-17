// include/utils/heap.h
#ifndef QUANT_TRADING_HEAP_H
#define QUANT_TRADING_HEAP_H

#include "../common.h"

typedef struct {
    void** elements;
    size_t size;
    size_t capacity;
    int (*compare)(const void*, const void*);
} Heap;

// Create new heap
Heap* heap_create(int (*compare)(const void*, const void*));

// Push element to heap
bool heap_push(Heap* heap, void* element);

// Pop top element from heap
void* heap_pop(Heap* heap);

// Peek at top element
void* heap_peek(const Heap* heap);

// Free heap
void heap_destroy(Heap* heap);

#endif // QUANT_TRADING_HEAP_H
