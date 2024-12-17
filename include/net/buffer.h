// include/net/buffer.h
#ifndef QUANT_TRADING_BUFFER_H
#define QUANT_TRADING_BUFFER_H

#include "../common.h"

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
    size_t read_pos;
    size_t write_pos;
} Buffer;

// Create new buffer
Buffer* buffer_create(size_t initial_capacity);

// Write data to buffer
bool buffer_write(Buffer* buffer, const uint8_t* data, size_t len);

// Read data from buffer
size_t buffer_read(Buffer* buffer, uint8_t* dst, size_t len);

// Free buffer
void buffer_destroy(Buffer* buffer);

#endif // QUANT_TRADING_BUFFER_H
