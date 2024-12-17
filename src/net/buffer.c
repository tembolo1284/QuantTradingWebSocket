#include "net/buffer.h"
#include <stdlib.h>
#include <string.h>

Buffer* buffer_create(size_t initial_capacity) {
    Buffer* buffer = malloc(sizeof(Buffer));
    if (!buffer) return NULL;

    buffer->data = malloc(initial_capacity);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }

    buffer->capacity = initial_capacity;
    buffer->size = 0;
    buffer->read_pos = 0;
    buffer->write_pos = 0;

    return buffer;
}

bool buffer_write(Buffer* buffer, const uint8_t* data, size_t len) {
    if (!buffer || !data) return false;
    
    // Check if we need to resize
    if (buffer->write_pos + len > buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        while (buffer->write_pos + len > new_capacity) {
            new_capacity *= 2;
        }
        
        uint8_t* new_data = realloc(buffer->data, new_capacity);
        if (!new_data) return false;
        
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    
    memcpy(buffer->data + buffer->write_pos, data, len);
    buffer->write_pos += len;
    buffer->size += len;
    
    return true;
}

size_t buffer_read(Buffer* buffer, uint8_t* dst, size_t len) {
    if (!buffer || !dst || !len) return 0;
    
    size_t available = buffer->size - buffer->read_pos;
    size_t to_read = len < available ? len : available;
    
    memcpy(dst, buffer->data + buffer->read_pos, to_read);
    buffer->read_pos += to_read;
    
    // Reset positions if buffer is empty
    if (buffer->read_pos == buffer->write_pos) {
        buffer->read_pos = buffer->write_pos = 0;
    }
    
    return to_read;
}

void buffer_destroy(Buffer* buffer) {
    if (buffer) {
        free(buffer->data);
        free(buffer);
    }
}
