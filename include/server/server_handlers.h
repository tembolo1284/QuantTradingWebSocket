#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

#include "ws_server.h"
#include "trading_engine/order_book.h"
#include "protocol/message_types.h"

typedef struct ServerHandlers ServerHandlers;

typedef struct {
    int thread_pool_size;
    int max_message_size;
    int message_queue_size;
} HandlerConfig;

ServerHandlers* server_handlers_create(const HandlerConfig* config);
void server_handlers_destroy(ServerHandlers* handlers);

// Message handling
int server_handlers_process_message(ServerHandlers* handlers, WSClient* client, const char* message, size_t len);
int server_handlers_broadcast_trade(ServerHandlers* handlers, const TradeMessage* trade);
int server_handlers_broadcast_status(ServerHandlers* handlers, const ServerStatus* status);

// Order book management
int server_handlers_add_order_book(ServerHandlers* handlers, const char* symbol);
OrderBook* server_handlers_get_order_book(ServerHandlers* handlers, const char* symbol);
int server_handlers_remove_order_book(ServerHandlers* handlers, const char* symbol);

// Thread pool control
int server_handlers_start_workers(ServerHandlers* handlers);
int server_handlers_stop_workers(ServerHandlers* handlers);

#endif /* SERVER_HANDLERS_H */
