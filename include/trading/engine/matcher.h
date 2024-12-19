#ifndef QUANT_TRADING_MATCHER_H
#define QUANT_TRADING_MATCHER_H

#include "trading/engine/order_book.h"
#include "common/types.h"
#include <stddef.h>

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

// Create or get order book for symbol
bool order_handler_create_book(const char* symbol);

// Add an order to the order book
OrderHandlingResult order_handler_add_order(const Order* order);

// Get book for specific symbol
OrderBook* order_handler_get_book_by_symbol(const char* symbol);

// Get array of all active order books
size_t order_handler_get_all_books(OrderBook** books, size_t max_books);

// Get current active book count
size_t order_handler_get_active_book_count(void);

// Legacy getter (returns first active book)
OrderBook* order_handler_get_book(void);

#endif // QUANT_TRADING_MATCHER_H
