#include "client/ws_client.h"
#include "utils/logging.h"
#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

struct WSClient {
    struct lws_context* context;
    struct lws* connection;
    pthread_t service_thread;
    bool running;
    bool connected;
    
    const char* host;
    int port;
    int reconnect_interval_ms;
    int ping_interval_ms;
    
    ConnectCallback connect_cb;
    DisconnectCallback disconnect_cb;
    MessageCallback message_cb;
    void* user_data;
    
    pthread_mutex_t lock;
};

static int callback_trading(struct lws* wsi, enum lws_callback_reasons reason,
                          void* user, void* in, size_t len) {
    WSClient* client = (WSClient*)user;
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            pthread_mutex_lock(&client->lock);
            client->connected = true;
            pthread_mutex_unlock(&client->lock);
            
            if (client->connect_cb) {
                client->connect_cb(client, client->user_data);
            }
            LOG_INFO("WebSocket connection established");
            break;
            
        case LWS_CALLBACK_CLIENT_CLOSED:
            pthread_mutex_lock(&client->lock);
            client->connected = false;
            pthread_mutex_unlock(&client->lock);
            
            if (client->disconnect_cb) {
                client->disconnect_cb(client, client->user_data);
            }
            LOG_INFO("WebSocket connection closed");
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (client->message_cb) {
                client->message_cb(client, (const char*)in, len, client->user_data);
            }
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            LOG_ERROR("WebSocket connection error");
            pthread_mutex_lock(&client->lock);
            client->connected = false;
            pthread_mutex_unlock(&client->lock);
            break;
            
        default:
            break;
    }
    
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        .name = "trading-protocol",
        .callback = callback_trading,
        .per_session_data_size = sizeof(WSClient),
        .rx_buffer_size = 4096,
        .tx_packet_size = 4096,
        .id = 0,
        .user = NULL,
    },
    {
        .name = NULL,
        .callback = NULL,
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
        .tx_packet_size = 0,
        .id = 0,
        .user = NULL,
    }
};

// In ws_client.c, modify the service thread
static void* service_thread(void* arg) {
    WSClient* client = (WSClient*)arg;
    
    int retry_count = 0;
    const int max_retries = 3;  // Limit reconnection attempts
    
    while (client->running) {
        lws_service(client->context, 50);
        
        if (!client->connected && client->running) {
            if (retry_count < max_retries) {
                LOG_INFO("Connection lost, attempting reconnect (%d/%d)...", 
                        retry_count + 1, max_retries);
                sleep(1);  // Wait before retry
                retry_count++;
            } else {
                LOG_ERROR("Max reconnection attempts reached, shutting down");
                client->running = false;
                break;
            }
        } else {
            retry_count = 0;  // Reset counter on successful connection
        }
    }
    
    return NULL;
}

WSClient* ws_client_create(const WSClientConfig* config) {
    WSClient* client = calloc(1, sizeof(WSClient));
    if (!client) {
        LOG_ERROR("Failed to allocate WebSocket client");
        return NULL;
    }
    
    client->host = strdup(config->server_host);
    client->port = config->server_port;
    client->reconnect_interval_ms = config->reconnect_interval_ms;
    client->ping_interval_ms = config->ping_interval_ms;
    
    pthread_mutex_init(&client->lock, NULL);
    
    struct lws_context_creation_info info = {
        .port = CONTEXT_PORT_NO_LISTEN,
        .protocols = protocols,
        .gid = -1,
        .uid = -1,
        .options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT
    };
    
    client->context = lws_create_context(&info);
    if (!client->context) {
        LOG_ERROR("Failed to create libwebsockets context");
        free((void*)client->host);
        free(client);
        return NULL;
    }
    
    LOG_INFO("WebSocket client created");
    return client;
}

void ws_client_destroy(WSClient* client) {
    if (!client) return;
    
    if (client->running) {
        ws_client_disconnect(client);
    }
    
    if (client->context) {
        lws_context_destroy(client->context);
    }
    
    pthread_mutex_destroy(&client->lock);
    free((void*)client->host);
    free(client);
    LOG_INFO("WebSocket client destroyed");
}

int ws_client_connect(WSClient* client) {
    if (!client) return -1;
    
    client->running = true;
    if (pthread_create(&client->service_thread, NULL, service_thread, client) != 0) {
        LOG_ERROR("Failed to create service thread");
        client->running = false;
        return -1;
    }
    
    struct lws_client_connect_info info = {
        .context = client->context,
        .address = client->host,
        .port = client->port,
        .path = "/",
        .protocol = "trading-protocol",
        .userdata = client
    };
    
    client->connection = lws_client_connect_via_info(&info);
    if (!client->connection) {
        LOG_ERROR("Failed to connect to WebSocket server");
        client->running = false;
        pthread_join(client->service_thread, NULL);
        return -1;
    }
    
    LOG_INFO("Connecting to WebSocket server %s:%d", client->host, client->port);
    return 0;
}

void ws_client_disconnect(WSClient* client) {
    if (!client || !client->running) return;
    
    client->running = false;
    pthread_join(client->service_thread, NULL);
    
    if (client->connection) {
        lws_cancel_service(client->context);
        client->connection = NULL;
    }
    
    LOG_INFO("WebSocket client disconnected");
}

int ws_client_send(WSClient* client, const char* message, size_t len) {
    if (!client || !message || !client->connected) return -1;
    
    unsigned char* buf = malloc(LWS_PRE + len);
    if (!buf) return -1;
    
    memcpy(buf + LWS_PRE, message, len);
    
    int result = lws_write(client->connection, buf + LWS_PRE, len, LWS_WRITE_TEXT);
    free(buf);
    
    if (result < 0) {
        LOG_ERROR("Failed to send WebSocket message");
        return -1;
    }
    
    return 0;
}

bool ws_client_is_connected(const WSClient* client) {
    return client && client->connected;
}

void ws_client_set_connect_callback(WSClient* client, ConnectCallback callback, 
                                  void* user_data) {
    if (!client) return;
    client->connect_cb = callback;
    client->user_data = user_data;
}

void ws_client_set_disconnect_callback(WSClient* client, DisconnectCallback callback,
                                     void* user_data) {
    if (!client) return;
    client->disconnect_cb = callback;
    client->user_data = user_data;
}

void ws_client_set_message_callback(WSClient* client, MessageCallback callback,
                                  void* user_data) {
    if (!client) return;
    client->message_cb = callback;
    client->user_data = user_data;
}
