#ifndef CLIENT_HANDLERS_H
#define CLIENT_HANDLERS_H

#include "ws_client.h"
#include "protocol/message_types.h"

typedef struct ClientHandlers ClientHandlers;

typedef struct {
    int max_message_queue;
    int process_interval_ms;
} HandlerConfig;

ClientHandlers* client_handlers_create(const HandlerConfig* config);
void client_handlers_destroy(ClientHandlers* handlers);

// Message handling
int client_handlers_process_message(ClientHandlers* handlers, const char* message, size_t len);
int client_handlers_submit_order(ClientHandlers* handlers, const OrderMessage* order);
int client_handlers_cancel_order(ClientHandlers* handlers, const char* order_id);

// Market data handling
int client_handlers_subscribe_symbol(ClientHandlers* handlers, const char* symbol);
int client_handlers_unsubscribe_symbol(ClientHandlers* handlers, const char* symbol);
int client_handlers_request_book(ClientHandlers* handlers, const char* symbol);

// Status tracking
bool client_handlers_is_server_ready(const ClientHandlers* handlers);
int64_t client_handlers_last_update_time(const ClientHandlers* handlers);

#endif /* CLIENT_HANDLERS_H */
