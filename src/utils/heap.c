#include "utils/heap.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

static void swap(void** a, void** b) {
    void* temp = *a;
    *a = *b;
    *b = temp;
}

static void heapify_up(Heap* heap, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (heap->compare(heap->elements[index], heap->elements[parent]) >= 0) {
            break;
        }
        swap(&heap->elements[index], &heap->elements[parent]);
        index = parent;
    }
}

static void heapify_down(Heap* heap, size_t index) {
    while (true) {
        size_t smallest = index;
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;

        if (left < heap->size && 
            heap->compare(heap->elements[left], heap->elements[smallest]) < 0) {
            smallest = left;
        }

        if (right < heap->size && 
            heap->compare(heap->elements[right], heap->elements[smallest]) < 0) {
            smallest = right;
        }

        if (smallest == index) {
            break;
        }

        swap(&heap->elements[index], &heap->elements[smallest]);
        index = smallest;
    }
}

Heap* heap_create(int (*compare)(const void*, const void*)) {
    if (!compare) return NULL;

    Heap* heap = malloc(sizeof(Heap));
    if (!heap) return NULL;

    heap->elements = malloc(INITIAL_CAPACITY * sizeof(void*));
    if (!heap->elements) {
        free(heap);
        return NULL;
    }

    heap->size = 0;
    heap->capacity = INITIAL_CAPACITY;
    heap->compare = compare;

    return heap;
}

bool heap_push(Heap* heap, void* element) {
    if (!heap || !element) return false;

    // Resize if needed
    if (heap->size == heap->capacity) {
        size_t new_capacity = heap->capacity * 2;
        void** new_elements = realloc(heap->elements, new_capacity * sizeof(void*));
        if (!new_elements) return false;

        heap->elements = new_elements;
        heap->capacity = new_capacity;
    }

    // Add element and maintain heap property
    heap->elements[heap->size] = element;
    heapify_up(heap, heap->size);
    heap->size++;

    return true;
}

void* heap_pop(Heap* heap) {
    if (!heap || heap->size == 0) return NULL;

    void* result = heap->elements[0];
    heap->size--;

    if (heap->size > 0) {
        heap->elements[0] = heap->elements[heap->size];
        heapify_down(heap, 0);
    }

    return result;
}

void* heap_peek(const Heap* heap) {
    if (!heap || heap->size == 0) return NULL;
    return heap->elements[0];
}

void heap_destroy(Heap* heap) {
    if (heap) {
        free(heap->elements);
        free(heap);
    }
}
