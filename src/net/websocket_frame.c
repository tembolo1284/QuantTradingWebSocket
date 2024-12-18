#include "net/websocket_frame.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

// WebSocket frame header structure
typedef struct {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;
    WebSocketFrameType opcode;
    bool masked;
    uint64_t payload_length;
    uint8_t mask_key[4];
} WebSocketFrameHeader;

// Decode frame header
static bool decode_frame_header(const uint8_t* data, size_t data_len, 
                                 WebSocketFrameHeader* header, 
                                 size_t* header_size) {
    if (data_len < 2) {
        LOG_ERROR("Insufficient data for WebSocket frame header");
        return false;
    }

    // First byte
    header->fin = (data[0] & 0x80) != 0;
    header->rsv1 = (data[0] & 0x40) != 0;
    header->rsv2 = (data[0] & 0x20) != 0;
    header->rsv3 = (data[0] & 0x10) != 0;
    header->opcode = (WebSocketFrameType)(data[0] & 0x0F);

    // Second byte
    header->masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;

    *header_size = 2;

    // Extended payload length
    if (payload_len == 126) {
        if (data_len < 4) {
            LOG_ERROR("Insufficient data for 16-bit payload length");
            return false;
        }
        payload_len = ((uint16_t)data[2] << 8) | data[3];
        *header_size = 4;
    } else if (payload_len == 127) {
        if (data_len < 10) {
            LOG_ERROR("Insufficient data for 64-bit payload length");
            return false;
        }
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        *header_size = 10;
    }

    header->payload_length = payload_len;

    // Mask key
    if (header->masked) {
        if (*header_size + 4 > data_len) {
            LOG_ERROR("Insufficient data for mask key");
            return false;
        }
        memcpy(header->mask_key, data + *header_size, 4);
        *header_size += 4;
    }

    return true;
}

// Unmask payload
static void unmask_payload(uint8_t* payload, size_t payload_len, 
                           const uint8_t* mask_key) {
    for (size_t i = 0; i < payload_len; i++) {
        payload[i] ^= mask_key[i % 4];
    }
}

// WebSocket frame decoding
bool ws_frame_decode(const uint8_t* raw_data, size_t raw_len, 
                     uint8_t** payload, size_t* payload_len, 
                     WebSocketFrameType* frame_type) {
    if (!raw_data || !payload || !payload_len || !frame_type) {
        LOG_ERROR("Invalid arguments to ws_frame_decode");
        return false;
    }

    WebSocketFrameHeader header;
    size_t header_size;

    // Decode frame header
    if (!decode_frame_header(raw_data, raw_len, &header, &header_size)) {
        LOG_ERROR("Failed to decode frame header");
        return false;
    }

    // Validate payload length
    if (header_size + header.payload_length > raw_len) {
        LOG_ERROR("Incomplete frame: expected %zu bytes, got %zu", 
                  header_size + header.payload_length, raw_len);
        return false;
    }

    // Allocate payload
    *payload = malloc(header.payload_length);
    if (!*payload) {
        LOG_ERROR("Failed to allocate memory for payload");
        return false;
    }

    // Copy payload
    memcpy(*payload, raw_data + header_size, header.payload_length);
    *payload_len = header.payload_length;
    *frame_type = header.opcode;

    // Unmask if needed
    if (header.masked) {
        unmask_payload(*payload, *payload_len, header.mask_key);
    }

    return true;
}

// WebSocket frame encoding
bool ws_frame_encode(const uint8_t* payload, size_t payload_len, 
                     WebSocketFrameType frame_type, 
                     uint8_t** encoded_frame, size_t* encoded_len) {
    if (!encoded_frame || !encoded_len) {
        LOG_ERROR("Invalid arguments to ws_frame_encode");
        return false;
    }

    // Calculate header size based on payload length
    size_t header_size = 2;  // Minimum header size
    if (payload_len > 125 && payload_len <= 65535) {
        header_size = 4;  // 16-bit extended payload length
    } else if (payload_len > 65535) {
        header_size = 10;  // 64-bit extended payload length
    }

    // Allocate buffer for entire frame
    *encoded_len = header_size + payload_len;
    *encoded_frame = malloc(*encoded_len);
    if (!*encoded_frame) {
        LOG_ERROR("Failed to allocate memory for encoded frame");
        return false;
    }

    // First byte: FIN flag set, no RSV flags, with opcode
    (*encoded_frame)[0] = 0x80 | (frame_type & 0x0F);

    // Second byte: no masking
    if (payload_len <= 125) {
        (*encoded_frame)[1] = payload_len;
    } else if (payload_len <= 65535) {
        (*encoded_frame)[1] = 126;
        (*encoded_frame)[2] = (payload_len >> 8) & 0xFF;
        (*encoded_frame)[3] = payload_len & 0xFF;
    } else {
        (*encoded_frame)[1] = 127;
        for (int i = 0; i < 8; i++) {
            (*encoded_frame)[2 + i] = (payload_len >> (56 - i * 8)) & 0xFF;
        }
    }

    // Copy payload
    if (payload && payload_len > 0) {
        memcpy(*encoded_frame + header_size, payload, payload_len);
    }

    return true;
}
