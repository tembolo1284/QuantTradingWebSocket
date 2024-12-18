#ifndef QUANT_TRADING_ORDER_HANDLER_H
#define QUANT_TRADING_ORDER_HANDLER_H

#include "order_book.h"
#include "net/websocket_server.h"
#include "net/websocket.h"

// Order handling result
typedef enum {
    ORDER_SUCCESS,
    ORDER_INVALID,
    ORDER_REJECTED
} OrderHandlingResult;

// Initialize order handling system
bool order_handler_init(void);

// Shutdown order handling system
void order_handler_shutdown(void);

// Require symbol when creating an order book
bool order_handler_create_book(const char* symbol);

// Add an order to the order book
OrderHandlingResult order_handler_add_order(const Order* order);

// Cancel an order from the order book
OrderHandlingResult order_handler_cancel_order(uint64_t order_id);

// Get the current order book
OrderBook* order_handler_get_book(void);

// Serialize order book to JSON
char* order_handler_serialize_book(void);

#endif // QUANT_TRADING_ORDER_HANDLER_H
