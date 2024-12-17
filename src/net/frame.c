#include "net/frame.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

static const char* frame_type_names[] = {
    "CONTINUATION",
    "TEXT",
    "BINARY",
    "RESERVED3",
    "RESERVED4",
    "RESERVED5",
    "RESERVED6",
    "RESERVED7",
    "CLOSE",
    "PING",
    "PONG"
};

WebSocketFrame* frame_create(const uint8_t* payload, size_t len, FrameType type) {
    LOG_DEBUG("Creating frame type=%s, payload_len=%zu", frame_type_names[type], len);
    
    WebSocketFrame* frame = calloc(1, sizeof(WebSocketFrame));
    if (!frame) {
        LOG_ERROR("Failed to allocate frame structure");
        return NULL;
    }
    
    frame->header.fin = true;
    frame->header.opcode = type;
    frame->header.payload_len = len;
    
    if (len > 0) {
        frame->payload = malloc(len);
        if (!frame->payload) {
            LOG_ERROR("Failed to allocate frame payload of size %zu", len);
            free(frame);
            return NULL;
        }
        memcpy(frame->payload, payload, len);
    }
    
    LOG_DEBUG("Frame created successfully");
    return frame;
}

static void generate_mask_key(uint8_t key[4]) {
    for (int i = 0; i < 4; i++) {
        key[i] = rand() & 0xFF;
    }
    LOG_DEBUG("Generated mask key: %02x %02x %02x %02x",
             key[0], key[1], key[2], key[3]);
}

static void mask_payload(uint8_t* payload, size_t len, const uint8_t mask_key[4]) {
    LOG_DEBUG("Masking payload of length %zu", len);
    for (size_t i = 0; i < len; i++) {
        payload[i] ^= mask_key[i % 4];
    }
}

uint8_t* frame_encode(const WebSocketFrame* frame, size_t* out_len) {
    if (!frame) {
        LOG_ERROR("Null frame provided to encode");
        return NULL;
    }
    
    LOG_DEBUG("Encoding frame type=%s, payload_len=%lu", 
             frame_type_names[frame->header.opcode],
             (unsigned long)frame->header.payload_len);

    // Calculate frame size
    size_t header_size = 2;  // Basic header
    if (frame->header.payload_len > 65535) {
        header_size += 8;
    } else if (frame->header.payload_len > 125) {
        header_size += 2;
    }

    if (frame->header.mask) {
        header_size += 4;  // Masking key
    }

    *out_len = header_size + frame->header.payload_len;
    
    LOG_DEBUG("Frame size: header=%zu, total=%zu", header_size, *out_len);

    // Allocate buffer
    uint8_t* buffer = malloc(*out_len);
    if (!buffer) {
        LOG_ERROR("Failed to allocate frame buffer of size %zu", *out_len);
        return NULL;
    }

    // Construct header
    buffer[0] = 0;
    if (frame->header.fin) buffer[0] |= 0x80;
    if (frame->header.rsv1) buffer[0] |= 0x40;
    if (frame->header.rsv2) buffer[0] |= 0x20;
    if (frame->header.rsv3) buffer[0] |= 0x10;
    buffer[0] |= frame->header.opcode & 0x0F;

    buffer[1] = frame->header.mask ? 0x80 : 0;

    // Set payload length
    size_t pos = 2;
    if (frame->header.payload_len <= 125) {
        buffer[1] |= frame->header.payload_len;
    } else if (frame->header.payload_len <= 65535) {
        buffer[1] |= 126;
        uint16_t len16 = htons((uint16_t)frame->header.payload_len);
        memcpy(buffer + pos, &len16, 2);
        pos += 2;
    } else {
        buffer[1] |= 127;
        uint64_t len64 = htobe64(frame->header.payload_len);
        memcpy(buffer + pos, &len64, 8);
        pos += 8;
    }

    // Add masking key if needed
    if (frame->header.mask) {
        generate_mask_key(buffer + pos);
        mask_payload(frame->payload, frame->header.payload_len, buffer + pos);
        pos += 4;
    }

    // Copy payload
    if (frame->payload && frame->header.payload_len > 0) {
        memcpy(buffer + pos, frame->payload, frame->header.payload_len);
    }

    LOG_DEBUG("Frame encoded successfully");
    return buffer;
}

FrameParseResult frame_parse(const uint8_t* data, size_t len, WebSocketFrame** out_frame) {
    FrameParseResult result = {0, false, NULL};
    
    if (len < 2) {
        LOG_DEBUG("Insufficient data for frame header (need 2 bytes, have %zu)", len);
        return result;
    }

    WebSocketFrame* frame = calloc(1, sizeof(WebSocketFrame));
    if (!frame) {
        LOG_ERROR("Failed to allocate frame structure");
        result.error = strdup("Memory allocation failed");
        return result;
    }

    // Parse basic header
    frame->header.fin = (data[0] & 0x80) != 0;
    frame->header.rsv1 = (data[0] & 0x40) != 0;
    frame->header.rsv2 = (data[0] & 0x20) != 0;
    frame->header.rsv3 = (data[0] & 0x10) != 0;
    frame->header.opcode = data[0] & 0x0F;
    frame->header.mask = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;

    LOG_DEBUG("Parsing frame: fin=%d, opcode=%s, mask=%d, initial_len=%lu",
             frame->header.fin, frame_type_names[frame->header.opcode],
             frame->header.mask, (unsigned long)payload_len);

    size_t pos = 2;

    // Parse extended payload length
    if (payload_len == 126) {
        if (len < pos + 2) {
            LOG_DEBUG("Insufficient data for 16-bit payload length");
            frame_destroy(frame);
            return result;
        }
        uint16_t len16;
        memcpy(&len16, data + pos, 2);
        frame->header.payload_len = ntohs(len16);
        pos += 2;
    } else if (payload_len == 127) {
        if (len < pos + 8) {
            LOG_DEBUG("Insufficient data for 64-bit payload length");
            frame_destroy(frame);
            return result;
        }
        uint64_t len64;
        memcpy(&len64, data + pos, 8);
        frame->header.payload_len = be64toh(len64);
        pos += 8;
    } else {
        frame->header.payload_len = payload_len;
    }

    LOG_DEBUG("Frame payload length: %lu", (unsigned long)frame->header.payload_len);

    // Read masking key
    if (frame->header.mask) {
        if (len < pos + 4) {
            LOG_DEBUG("Insufficient data for masking key");
            frame_destroy(frame);
            return result;
        }
        memcpy(frame->header.mask_key, data + pos, 4);
        pos += 4;
        LOG_DEBUG("Masking key: %02x %02x %02x %02x",
                 frame->header.mask_key[0], frame->header.mask_key[1],
                 frame->header.mask_key[2], frame->header.mask_key[3]);
    }

    // Check if we have the full payload
    if (len < pos + frame->header.payload_len) {
        LOG_DEBUG("Insufficient data for payload (need %lu more bytes)",
                 (unsigned long)(pos + frame->header.payload_len - len));
        frame_destroy(frame);
        return result;
    }

    // Copy and unmask payload if present
    if (frame->header.payload_len > 0) {
        frame->payload = malloc(frame->header.payload_len);
        if (!frame->payload) {
            LOG_ERROR("Failed to allocate payload of size %lu",
                     (unsigned long)frame->header.payload_len);
            frame_destroy(frame);
            result.error = strdup("Memory allocation failed");
            return result;
        }
        memcpy(frame->payload, data + pos, frame->header.payload_len);
        
        if (frame->header.mask) {
            mask_payload(frame->payload, frame->header.payload_len,
                        frame->header.mask_key);
            LOG_DEBUG("Payload unmasked");
        }
    }

    result.bytes_consumed = pos + frame->header.payload_len;
    result.complete = true;
    *out_frame = frame;

    LOG_DEBUG("Frame parsed successfully, consumed %zu bytes", result.bytes_consumed);
    return result;
}

bool frame_validate(const WebSocketFrame* frame) {
    if (!frame) {
        LOG_ERROR("Null frame provided for validation");
        return false;
    }

    // Check RSV bits
    if (frame->header.rsv1 || frame->header.rsv2 || frame->header.rsv3) {
        LOG_ERROR("Reserved bits must be 0");
        return false;
    }

    // Validate opcode
    if (frame->header.opcode > 0x0A) {
        LOG_ERROR("Invalid opcode: %d", frame->header.opcode);
        return false;
    }

    // Control frames must not be fragmented and must have payload <= 125 bytes
    if (frame->header.opcode >= 0x08) {
        if (!frame->header.fin) {
            LOG_ERROR("Control frames must not be fragmented");
            return false;
        }
        if (frame->header.payload_len > 125) {
            LOG_ERROR("Control frame payload too large: %lu bytes",
                     (unsigned long)frame->header.payload_len);
            return false;
        }
    }

    LOG_DEBUG("Frame validation successful");
    return true;
}

void frame_destroy(WebSocketFrame* frame) {
    if (!frame) return;
    
    LOG_DEBUG("Destroying frame type=%s, payload_len=%lu",
             frame_type_names[frame->header.opcode],
             (unsigned long)frame->header.payload_len);
    
    free(frame->payload);
    free(frame);
}

const char* frame_type_string(FrameType type) {
    if (type > 0x0A) {
        LOG_ERROR("Invalid frame type: %d", type);
        return "UNKNOWN";
    }
    return frame_type_names[type];
}

size_t frame_calculate_header_size(uint64_t payload_len) {
    size_t size = 2;  // Basic header
    
    if (payload_len > 65535) {
        size += 8;
    } else if (payload_len > 125) {
        size += 2;
    }
    
    LOG_DEBUG("Calculated header size: %zu bytes for payload length %lu",
             size, (unsigned long)payload_len);
    return size;
}
