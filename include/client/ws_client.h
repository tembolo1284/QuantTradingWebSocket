#ifndef CLIENT_WS_CLIENT_H
#define CLIENT_WS_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct WSClient WSClient;

typedef struct {
    const char* server_host;
    int server_port;
    int reconnect_interval_ms;
    int ping_interval_ms;
} WSClientConfig;

// Client lifecycle
WSClient* ws_client_create(const WSClientConfig* config);
int ws_client_connect(WSClient* client);
void ws_client_disconnect(WSClient* client);
void ws_client_destroy(WSClient* client);

// Message operations
int ws_client_send(WSClient* client, const char* message, size_t len);
bool ws_client_is_connected(const WSClient* client);

// Callback types
typedef void (*ConnectCallback)(WSClient* client, void* user_data);
typedef void (*DisconnectCallback)(WSClient* client, void* user_data);
typedef void (*MessageCallback)(WSClient* client, const char* message, size_t len, void* user_data);

// Set callbacks
void ws_client_set_connect_callback(WSClient* client, ConnectCallback callback, void* user_data);
void ws_client_set_disconnect_callback(WSClient* client, DisconnectCallback callback, void* user_data);
void ws_client_set_message_callback(WSClient* client, MessageCallback callback, void* user_data);

#endif /* CLIENT_WS_CLIENT_H */
