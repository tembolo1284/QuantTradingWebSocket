#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include "common/types.h"
#include "net/buffer.h"

// Callback function types
typedef void (*WebSocketMessageCallback)(const uint8_t* data, size_t len, void* user_data);
typedef void (*WebSocketErrorCallback)(ErrorCode error_code, void* user_data);

typedef struct {
    WebSocketMessageCallback on_message;
    WebSocketErrorCallback on_error;
    void* user_data;
} WebSocketCallbacks;

// WebSocket structure
typedef struct WebSocket {
    int sock_fd;
    Buffer* recv_buffer;
    Buffer* send_buffer;
    WebSocketCallbacks callbacks;
    bool connected;
    char* host;
    uint16_t port;
    uint64_t message_count;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    time_t last_message_time;
} WebSocket;

// Core WebSocket functions
WebSocket* ws_create(const char* host, uint16_t port, const WebSocketCallbacks* callbacks);
void ws_process(WebSocket* ws);
bool ws_send(WebSocket* ws, const uint8_t* data, size_t len);
void ws_close(WebSocket* ws);
bool ws_is_connected(const WebSocket* ws);

// Error handling
const char* ws_error_string(ErrorCode error);

#endif // WEBSOCKET_H
