#include "trading/order_handler.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

// Static order book to manage orders
static OrderBook* global_order_book = NULL;
static char global_symbol[16] = {0};  // Remove default symbol

bool order_handler_init(void) {
    // Cleanup existing book if any
    if (global_order_book) {
        order_book_destroy(global_order_book);
    }

    global_order_book = NULL;
    memset(global_symbol, 0, sizeof(global_symbol));
    return true;
}

bool order_handler_create_book(const char* symbol) {
    // Require symbol to be provided
    if (!symbol || symbol[0] == '\0') {
        LOG_ERROR("Symbol must be provided when creating order book");
        return false;
    }

    // Cleanup existing book if any
    if (global_order_book) {
        order_book_destroy(global_order_book);
    }

    // Create new order book
    global_order_book = order_book_create(symbol);
    
    if (!global_order_book) {
        LOG_ERROR("Failed to create order book for symbol: %s", symbol);
        return false;
    }

    // Copy symbol
    strncpy(global_symbol, symbol, sizeof(global_symbol) - 1);
    global_symbol[sizeof(global_symbol) - 1] = '\0';

    LOG_INFO("Order book created/switched to symbol: %s", global_symbol);
    return true;
}

void order_handler_shutdown(void) {
    if (global_order_book) {
        order_book_destroy(global_order_book);
        global_order_book = NULL;
    }
    memset(global_symbol, 0, sizeof(global_symbol));
}

OrderHandlingResult order_handler_add_order(const Order* order) {
    if (!global_order_book || !order) {
        LOG_ERROR("Invalid order book or order");
        return ORDER_INVALID;
    }

    // Validate symbol match
    if (strcmp(global_symbol, order->symbol) != 0) {
        LOG_ERROR("Symbol mismatch. Current book symbol: %s, Order symbol: %s", 
                  global_symbol, order->symbol);
        return ORDER_INVALID;
    }

    // Attempt to add order
    if (order_book_add(global_order_book, order)) {
        LOG_INFO("Order added successfully: price=%.2f, quantity=%u, is_buy=%d", 
                 order->price, order->quantity, order->is_buy);
        return ORDER_SUCCESS;
    } else {
        LOG_ERROR("Failed to add order to order book");
        return ORDER_REJECTED;
    }
}

// Get current order book
OrderBook* order_handler_get_book(void) {
    return global_order_book;
}
