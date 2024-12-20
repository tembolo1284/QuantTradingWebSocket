#ifndef QUANT_TRADING_WEBSOCKET_SERVER_H
#define QUANT_TRADING_WEBSOCKET_SERVER_H

#include "common/types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Forward declarations
typedef struct WebSocketServer WebSocketServer;
typedef struct WebSocketClient WebSocketClient;

// Callback types
typedef void (*OnClientConnectCallback)(WebSocketClient* client);
typedef void (*OnClientDisconnectCallback)(WebSocketClient* client);
typedef void (*OnClientMessageCallback)(WebSocketClient* client, const uint8_t* data, size_t len);

// Server configuration
typedef struct {
    uint16_t port;
    OnClientConnectCallback on_client_connect;
    OnClientDisconnectCallback on_client_disconnect;
    OnClientMessageCallback on_client_message;
} WebSocketServerConfig;

// Server API
WebSocketServer* ws_server_create(const WebSocketServerConfig* config);
void ws_server_process(WebSocketServer* server);
void ws_server_broadcast(WebSocketServer* server, const uint8_t* data, size_t len);
void ws_server_destroy(WebSocketServer* server);

void ws_server_request_shutdown(WebSocketServer* server);

// Client API (for sending responses)
void ws_client_send(WebSocketClient* client, const uint8_t* data, size_t len);
void ws_client_close(WebSocketClient* client);
void* ws_client_get_user_data(const WebSocketClient* client);
void ws_client_set_user_data(WebSocketClient* client, void* data);

#endif // QUANT_TRADING_WEBSOCKET_SERVER_H