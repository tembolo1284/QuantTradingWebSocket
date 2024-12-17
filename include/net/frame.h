// include/net/frame.h
#ifndef QUANT_TRADING_FRAME_H
#define QUANT_TRADING_FRAME_H

#include "../common.h"

typedef enum {
    FRAME_CONTINUATION = 0x0,
    FRAME_TEXT = 0x1,
    FRAME_BINARY = 0x2,
    FRAME_CLOSE = 0x8,
    FRAME_PING = 0x9,
    FRAME_PONG = 0xA
} FrameType;

typedef struct {
    FrameType type;
    bool fin;
    bool masked;
    uint64_t payload_length;
    uint8_t mask_key[4];
    uint8_t* payload;
} WebSocketFrame;

// Parse WebSocket frame from raw data
int frame_parse(const uint8_t* data, size_t len, WebSocketFrame* frame);

// Create WebSocket frame
int frame_create(WebSocketFrame* frame, const uint8_t* payload, size_t len, FrameType type);

#endif // QUANT_TRADING_FRAME_H
