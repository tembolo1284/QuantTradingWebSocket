#include "server/ws_server.h"
#include "utils/logging.h"
#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

struct WSServer {
    struct lws_context* context;
    struct lws_vhost* vhost;
    const WSServerConfig* config;
    
    // Callbacks
    ClientConnectCallback connect_cb;
    ClientDisconnectCallback disconnect_cb;
    MessageCallback message_cb;
    void* user_data;
    
    // Threading
    pthread_t service_thread;
    bool running;
    pthread_mutex_t lock;
};

struct WSClient {
    struct lws* wsi;
    WSClientInfo info;
    WSServer* server;
};

// Forward declarations
static void* service_thread(void* arg);
static int callback_trading(struct lws* wsi, enum lws_callback_reasons reason,
                          void* user, void* in, size_t len);

static struct lws_protocols protocols[] = {
    {
        "trading-protocol",
        callback_trading,
        sizeof(WSClient),
        4096,  // rx buffer size
    },
    { NULL, NULL, 0, 0 }
};

WSServer* ws_server_create(const WSServerConfig* config) {
    WSServer* server = calloc(1, sizeof(WSServer));
    if (!server) {
        LOG_ERROR("Failed to allocate server structure");
        return NULL;
    }

    server->config = config;
    pthread_mutex_init(&server->lock, NULL);

    struct lws_context_creation_info info = {
        .port = config->port,
        .protocols = protocols,
        .gid = -1,
        .uid = -1,
        .options = LWS_SERVER_OPTION_VALIDATE_UTF8
    };

    server->context = lws_create_context(&info);
    if (!server->context) {
        LOG_ERROR("Failed to create libwebsockets context");
        free(server);
        return NULL;
    }

    LOG_INFO("WebSocket server created on port %d", config->port);
    return server;
}

void ws_server_destroy(WSServer* server) {
    if (!server) return;

    if (server->running) {
        ws_server_stop(server);
    }

    if (server->context) {
        lws_context_destroy(server->context);
    }

    pthread_mutex_destroy(&server->lock);
    free(server);
    LOG_INFO("WebSocket server destroyed");
}

int ws_server_start(WSServer* server) {
    if (!server) return -1;

    server->running = true;
    if (pthread_create(&server->service_thread, NULL, service_thread, server) != 0) {
        LOG_ERROR("Failed to create service thread");
        server->running = false;
        return -1;
    }

    LOG_INFO("WebSocket server started");
    return 0;
}

void ws_server_stop(WSServer* server) {
    if (!server || !server->running) return;

    server->running = false;
    pthread_join(server->service_thread, NULL);
    LOG_INFO("WebSocket server stopped");
}

static void* service_thread(void* arg) {
    WSServer* server = (WSServer*)arg;
    
    while (server->running) {
        lws_service(server->context, 50);  // 50ms timeout
    }
    
    return NULL;
}

static int callback_trading(struct lws* wsi, enum lws_callback_reasons reason,
                          void* user, void* in, size_t len) {
    WSClient* client = (WSClient*)user;
    WSServer* server = lws_get_context_user(lws_get_context(wsi));

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            LOG_DEBUG("Protocol initialized");
            break;

        case LWS_CALLBACK_ESTABLISHED: {
            client->wsi = wsi;
            client->server = server;
            snprintf(client->info.client_id, sizeof(client->info.client_id), 
                    "client-%p", (void*)wsi);
            client->info.connect_time = time(NULL);
            
            if (server->connect_cb) {
                server->connect_cb(client, server->user_data);
            }
            LOG_INFO("Client connected: %s", client->info.client_id);
            break;
        }

        case LWS_CALLBACK_CLOSED: {
            if (server->disconnect_cb) {
                server->disconnect_cb(client, server->user_data);
            }
            LOG_INFO("Client disconnected: %s", client->info.client_id);
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            if (server->message_cb) {
                server->message_cb(client, (const char*)in, len, server->user_data);
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

int ws_server_broadcast(WSServer* server, const char* message, size_t len) {
    if (!server || !message) return -1;

    // Need to allocate LWS_PRE bytes before the message
    unsigned char* buf = malloc(LWS_PRE + len);
    if (!buf) return -1;

    memcpy(buf + LWS_PRE, message, len);
    
    pthread_mutex_lock(&server->lock);
    lws_start_foreach_llp(struct lws**, ppwsi, 
                         lws_get_peer_wait_writeable(server->vhost)) {
        lws_write(*ppwsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);
    } lws_end_foreach_llp(ppwsi, next);
    pthread_mutex_unlock(&server->lock);

    free(buf);
    return 0;
}

int ws_server_send(WSClient* client, const char* message, size_t len) {
    if (!client || !message) return -1;

    unsigned char* buf = malloc(LWS_PRE + len);
    if (!buf) return -1;

    memcpy(buf + LWS_PRE, message, len);
    lws_write(client->wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);
    free(buf);
    return 0;
}

void ws_server_set_connect_callback(WSServer* server, 
                                  ClientConnectCallback callback, 
                                  void* user_data) {
    if (!server) return;
    server->connect_cb = callback;
    server->user_data = user_data;
}

void ws_server_set_disconnect_callback(WSServer* server, 
                                     ClientDisconnectCallback callback, 
                                     void* user_data) {
    if (!server) return;
    server->disconnect_cb = callback;
    server->user_data = user_data;
}

void ws_server_set_message_callback(WSServer* server, 
                                  MessageCallback callback, 
                                  void* user_data) {
    if (!server) return;
    server->message_cb = callback;
    server->user_data = user_data;
}

const WSClientInfo* ws_server_get_client_info(const WSClient* client) {
    return client ? &client->info : NULL;
}