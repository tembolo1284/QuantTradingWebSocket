#ifndef QUANT_TRADING_WEBSOCKET_FRAME_H
#define QUANT_TRADING_WEBSOCKET_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// WebSocket frame types
typedef enum {
    WS_FRAME_CONTINUATION = 0x0,
    WS_FRAME_TEXT = 0x1,
    WS_FRAME_BINARY = 0x2,
    WS_FRAME_CLOSE = 0x8,
    WS_FRAME_PING = 0x9,
    WS_FRAME_PONG = 0xA
} WebSocketFrameType;

// Frame decoding functions to be implemented
bool ws_frame_decode(const uint8_t* raw_data, size_t raw_len, 
                     uint8_t** payload, size_t* payload_len, 
                     WebSocketFrameType* frame_type);

bool ws_frame_encode(const uint8_t* payload, size_t payload_len, 
                     WebSocketFrameType frame_type, 
                     uint8_t** encoded_frame, size_t* encoded_len);

#endif // QUANT_TRADING_WEBSOCKET_FRAME_H
