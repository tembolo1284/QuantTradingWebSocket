#ifndef QUANT_TRADING_FRAME_H
#define QUANT_TRADING_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Frame types as defined in RFC 6455
typedef enum {
    FRAME_CONTINUATION = 0x0,
    FRAME_TEXT = 0x1,
    FRAME_BINARY = 0x2,
    FRAME_CLOSE = 0x8,
    FRAME_PING = 0x9,
    FRAME_PONG = 0xA
} FrameType;

// Frame header structure
typedef struct {
    bool fin;              // Final fragment flag
    bool rsv1;             // Reserved bit 1
    bool rsv2;             // Reserved bit 2
    bool rsv3;             // Reserved bit 3
    bool mask;             // Masking flag
    FrameType opcode;      // Frame type
    uint64_t payload_len;  // Payload length
    uint8_t mask_key[4];   // Masking key (if mask is true)
} FrameHeader;

// Complete frame structure
typedef struct {
    FrameHeader header;
    uint8_t* payload;
} WebSocketFrame;

// Frame parsing result
typedef struct {
    int bytes_consumed;   // Number of bytes processed
    bool complete;        // Whether the frame is complete
    char* error;         // Error message if any
} FrameParseResult;

// Create a new WebSocket frame
WebSocketFrame* frame_create(const uint8_t* payload, size_t len, FrameType type);

// Parse incoming data into a frame
FrameParseResult frame_parse(const uint8_t* data, size_t len, WebSocketFrame** frame);

// Encode a frame for sending
uint8_t* frame_encode(const WebSocketFrame* frame, size_t* out_len);

// Validate a frame
bool frame_validate(const WebSocketFrame* frame);

// Clean up a frame
void frame_destroy(WebSocketFrame* frame);

// Utility functions
size_t frame_calculate_header_size(uint64_t payload_len);
const char* frame_type_string(FrameType type);

#endif // QUANT_TRADING_FRAME_H
