#ifndef QUANT_TRADING_WEBSOCKET_H
#define QUANT_TRADING_WEBSOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

// Error codes for WebSocket operations
typedef enum {
    WS_ERROR_NONE = 0,
    WS_ERROR_CONNECTION_FAILED,
    WS_ERROR_HANDSHAKE_FAILED,
    WS_ERROR_INVALID_FRAME,
    WS_ERROR_SEND_FAILED,
    WS_ERROR_MEMORY,
    WS_ERROR_TIMEOUT
} WebSocketError;

// Main WebSocket API functions
WebSocket* ws_create(const char* host, uint16_t port, const WebSocketCallbacks* callbacks);
bool ws_send(WebSocket* ws, const uint8_t* data, size_t len);
void ws_process(WebSocket* ws);
void ws_close(WebSocket* ws);

// Helper functions
const char* ws_error_string(WebSocketError error);
bool ws_is_connected(const WebSocket* ws);

#endif // QUANT_TRADING_WEBSOCKET_H
