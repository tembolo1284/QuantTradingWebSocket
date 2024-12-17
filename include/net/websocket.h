// include/net/websocket.h
#ifndef QUANT_TRADING_WEBSOCKET_H
#define QUANT_TRADING_WEBSOCKET_H

#include "../common.h"

#define MAX_FRAME_SIZE 65536
#define MAX_PAYLOAD_SIZE (MAX_FRAME_SIZE - 14)  // Maximum WebSocket frame size minus header

typedef struct WebSocket WebSocket;

typedef void (*OnMessageCallback)(const uint8_t* data, size_t len, void* user_data);
typedef void (*OnErrorCallback)(ErrorCode error, void* user_data);

typedef struct {
    OnMessageCallback on_message;
    OnErrorCallback on_error;
    void* user_data;
} WebSocketCallbacks;

// Create new WebSocket connection
WebSocket* ws_create(const char* host, uint16_t port, const WebSocketCallbacks* callbacks);

// Send data through WebSocket
bool ws_send(WebSocket* ws, const uint8_t* data, size_t len);

// Process incoming WebSocket data
void ws_process(WebSocket* ws);

// Close WebSocket connection
void ws_close(WebSocket* ws);

#endif // QUANT_TRADING_WEBSOCKET_H
