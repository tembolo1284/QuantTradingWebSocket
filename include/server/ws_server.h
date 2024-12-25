#ifndef SERVER_WS_SERVER_H
#define SERVER_WS_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct WSServer WSServer;
typedef struct WSClient WSClient;

// Server configuration
typedef struct {
    const char* host;
    int port;
    int max_clients;
    int ping_interval_ms;
    int status_interval_ms;
} WSServerConfig;

// Client connection info
typedef struct {
    char client_id[32];
    char* subscribed_symbols;  // Comma-separated list
    int64_t connect_time;
    int64_t last_ping_time;
} WSClientInfo;

// Initialize server
WSServer* ws_server_create(const WSServerConfig* config);

// Start server (non-blocking)
int ws_server_start(WSServer* server);

// Stop server
void ws_server_stop(WSServer* server);

// Clean up
void ws_server_destroy(WSServer* server);

// Broadcast message to all clients
int ws_server_broadcast(WSServer* server, const char* message, size_t len);

// Send message to specific client
int ws_server_send(WSClient* client, const char* message, size_t len);

// Get client info
const WSClientInfo* ws_server_get_client_info(const WSClient* client);

// Get number of connected clients
int ws_server_get_client_count(const WSServer* server);

// Callback types
typedef void (*ClientConnectCallback)(WSClient* client, void* user_data);
typedef void (*ClientDisconnectCallback)(WSClient* client, void* user_data);
typedef void (*MessageCallback)(WSClient* client, const char* message, size_t len, void* user_data);

// Set callbacks
void ws_server_set_connect_callback(WSServer* server, ClientConnectCallback callback, void* user_data);
void ws_server_set_disconnect_callback(WSServer* server, ClientDisconnectCallback callback, void* user_data);
void ws_server_set_message_callback(WSServer* server, MessageCallback callback, void* user_data);

#endif /* SERVER_WS_SERVER_H */
