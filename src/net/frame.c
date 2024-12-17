#include "net/frame.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

static void unmask_payload(uint8_t* payload, size_t len, const uint8_t mask_key[4]) {
    for (size_t i = 0; i < len; i++) {
        payload[i] ^= mask_key[i % 4];
    }
}

int frame_parse(const uint8_t* data, size_t len, WebSocketFrame* frame) {
    if (!data || !frame || len < 2) return -1;
    
    size_t pos = 0;
    
    // Parse first byte
    frame->fin = (data[pos] & 0x80) != 0;
    frame->type = (FrameType)(data[pos] & 0x0F);
    pos++;
    
    // Parse second byte
    frame->masked = (data[pos] & 0x80) != 0;
    uint8_t payload_len = data[pos] & 0x7F;
    pos++;
    
    // Parse extended payload length
    if (payload_len == 126) {
        if (len < pos + 2) return -1;
        uint16_t len16;
        memcpy(&len16, data + pos, 2);
        frame->payload_length = ntohs(len16);
        pos += 2;
    } else if (payload_len == 127) {
        if (len < pos + 8) return -1;
        uint64_t len64;
        memcpy(&len64, data + pos, 8);
        frame->payload_length = be64toh(len64);
        pos += 8;
    } else {
        frame->payload_length = payload_len;
    }
    
    // Parse masking key
    if (frame->masked) {
        if (len < pos + 4) return -1;
        memcpy(frame->mask_key, data + pos, 4);
        pos += 4;
    }
    
    // Check if we have the full payload
    if (len < pos + frame->payload_length) return -1;
    
    // Copy and unmask payload if needed
    frame->payload = malloc(frame->payload_length);
    if (!frame->payload) return -1;
    
    memcpy(frame->payload, data + pos, frame->payload_length);
    if (frame->masked) {
        unmask_payload(frame->payload, frame->payload_length, frame->mask_key);
    }
    
    return pos + frame->payload_length;
}

int frame_create(WebSocketFrame* frame, const uint8_t* payload, size_t len, FrameType type) {
    if (!frame || (!payload && len > 0)) return -1;
    
    frame->type = type;
    frame->fin = true;
    frame->masked = false;  // Server frames are not masked
    frame->payload_length = len;
    
    if (len > 0) {
        frame->payload = malloc(len);
        if (!frame->payload) return -1;
        memcpy(frame->payload, payload, len);
    } else {
        frame->payload = NULL;
    }
    
    return 0;
}
