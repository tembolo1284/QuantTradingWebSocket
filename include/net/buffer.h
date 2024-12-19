#ifndef QUANT_TRADING_BUFFER_H
#define QUANT_TRADING_BUFFER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct Buffer {
    uint8_t* data;
    size_t capacity;
    size_t size;
    size_t read_pos;
    size_t write_pos;  // Added this field
} Buffer;

// Create a new buffer with initial capacity
Buffer* buffer_create(size_t initial_capacity);

// Destroy buffer and free memory
void buffer_destroy(Buffer* buffer);

// Write data to buffer
bool buffer_write(Buffer* buffer, const uint8_t* data, size_t len);

// Read data from buffer
size_t buffer_read(Buffer* buffer, uint8_t* dst, size_t len);

// Resize buffer to new capacity
bool buffer_resize(Buffer* buffer, size_t new_capacity);

#endif // QUANT_TRADING_BUFFER_H
