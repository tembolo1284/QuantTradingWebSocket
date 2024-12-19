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
        uint8_t* masked_payload = malloc(frame->header.payload_len);
        if (masked_payload) {
            memcpy(masked_payload, frame->payload, frame->header.payload_len);
            mask_payload(masked_payload, frame->header.payload_len, buffer + pos);
            memcpy(buffer + pos + 4, masked_payload, frame->header.payload_len);
            free(masked_payload);
        }
        pos += 4;
    } else {
        // Copy payload without masking
        if (frame->payload && frame->header.payload_len > 0) {
            memcpy(buffer + pos, frame->payload, frame->header.payload_len);
        }
    }

    LOG_DEBUG("Frame encoded successfully");
    return buffer;
}

FrameParseResult frame_parse(const uint8_t* data, size_t len, WebSocketFrame** out_frame) {
    FrameParseResult result = {0, false, NULL};
    *out_frame = NULL;

    // Check minimum frame size
    if (len < 2) {
        LOG_DEBUG("Insufficient data for frame header (need 2 bytes, have %zu)", len);
        return result;
    }

    // Calculate total header size
    size_t header_size = 2;
    uint8_t payload_len_initial = data[1] & 0x7F;
    bool is_masked = (data[1] & 0x80) != 0;

    if (payload_len_initial == 126) {
        header_size += 2;
    } else if (payload_len_initial == 127) {
        header_size += 8;
    }
    if (is_masked) {
        header_size += 4;
    }

    // Check if we have the complete header
    if (len < header_size) {
        LOG_DEBUG("Insufficient data for complete header (need %zu bytes, have %zu)",
                 header_size, len);
        return result;
    }

    // Create frame
    WebSocketFrame* frame = calloc(1, sizeof(WebSocketFrame));
    if (!frame) {
        LOG_ERROR("Failed to allocate frame structure");
        result.error = strdup("Memory allocation failed");
        return result;
    }

    // Parse header
    frame->header.fin = (data[0] & 0x80) != 0;
    frame->header.opcode = data[0] & 0x0F;
    frame->header.mask = is_masked;

    // Get payload length
    size_t pos = 2;
    if (payload_len_initial == 126) {
        uint16_t len16;
        memcpy(&len16, data + pos, 2);
        frame->header.payload_len = ntohs(len16);
        pos += 2;
    } else if (payload_len_initial == 127) {
        uint64_t len64;
        memcpy(&len64, data + pos, 8);
        frame->header.payload_len = be64toh(len64);
        pos += 8;
    } else {
        frame->header.payload_len = payload_len_initial;
    }

    // Get masking key if present
    if (frame->header.mask) {
        memcpy(frame->header.mask_key, data + pos, 4);
        pos += 4;
    }

    // Check if we have the complete frame
    if (len < pos + frame->header.payload_len) {
        LOG_DEBUG("Insufficient data for complete frame (need %zu bytes, have %zu)",
                 pos + frame->header.payload_len, len);
        frame_destroy(frame);
        return result;
    }

    // Allocate and copy payload
    if (frame->header.payload_len > 0) {
        frame->payload = malloc(frame->header.payload_len);
        if (!frame->payload) {
            LOG_ERROR("Failed to allocate payload");
            frame_destroy(frame);
            result.error = strdup("Memory allocation failed");
            return result;
        }
        memcpy(frame->payload, data + pos, frame->header.payload_len);

        if (frame->header.mask) {
            mask_payload(frame->payload, frame->header.payload_len, 
                        frame->header.mask_key);
        }
    }

    // Success
    result.bytes_consumed = pos + frame->header.payload_len;
    result.complete = true;
    *out_frame = frame;

    LOG_DEBUG("Successfully parsed complete frame of %zu bytes", result.bytes_consumed);
    return result;
}

bool frame_validate(const WebSocketFrame* frame) {
    if (!frame) {
        LOG_ERROR("Null frame provided for validation");
        return false;
    }

    // Check RSV bits (should be 0)
    if (frame->header.rsv1 || frame->header.rsv2 || frame->header.rsv3) {
        LOG_ERROR("Reserved bits must be 0");
        return false;
    }

    // Validate opcode
    if (frame->header.opcode > 0x0A) {
        LOG_ERROR("Invalid opcode: %d", frame->header.opcode);
        return false;
    }

    // Control frames (0x8-0xF) must not be fragmented and payload <= 125
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

    return size;
}
