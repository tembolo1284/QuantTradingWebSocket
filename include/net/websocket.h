#ifndef QUANT_TRADING_WEBSOCKET_H
#define QUANT_TRADING_WEBSOCKET_H

#include "common/types.h"
#include <stdint.h>
#include <stddef.h>

// Forward declarations
typedef struct WebSocket WebSocket;

// Callback function types
typedef void (*OnMessageCallback)(const uint8_t* data, size_t len, void* user_data);
typedef void (*OnErrorCallback)(ErrorCode error, void* user_data);

typedef struct {
    OnMessageCallback on_message;
    OnErrorCallback on_error;
    void* user_data;
} WebSocketCallbacks;

// Main WebSocket API functions
WebSocket* ws_create(const char* host, uint16_t port, const WebSocketCallbacks* callbacks);
bool ws_send(WebSocket* ws, const uint8_t* data, size_t len);
void ws_process(WebSocket* ws);
void ws_close(WebSocket* ws);
bool ws_is_connected(const WebSocket* ws);

// Error string conversion
const char* ws_error_string(ErrorCode error);

#endif // QUANT_TRADING_WEBSOCKET_H
