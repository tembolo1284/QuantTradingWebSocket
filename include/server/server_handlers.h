#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

#include "ws_server.h"
#include "trading_engine/order.h"
#include "trading_engine/order_book.h"
#include "protocol/message_types.h"
#include "trading_engine/trade_broadcaster.h"
#include <cJSON/cJSON.h>

typedef struct ServerHandlers ServerHandlers;

// Handler configuration
typedef struct {
    int thread_pool_size;
    int max_message_size;
    int message_queue_size;
    TradeBroadcaster* trade_broadcaster;
} HandlerConfig;

// Message handler function type
typedef int (*MessageHandler)(ServerHandlers* handlers, WSClient* client, const cJSON* root, char* response);

// Handler functions
int handle_place_order(ServerHandlers* handlers, WSClient* client, const cJSON* root, char* response);
int handle_cancel_order(ServerHandlers* handlers, WSClient* client, const cJSON* root, char* response);
int handle_book_request(ServerHandlers* handlers, WSClient* client, const cJSON* root, char* response);

// Constructor/Destructor
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

// Helper functions
int send_error_response(WSClient* client, const char* error_msg, char* response);
char* dequeue_message(ServerHandlers* handlers, WSClient** client);

#endif /* SERVER_HANDLERS_H */
