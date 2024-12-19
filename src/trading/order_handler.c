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

void order_handler_shutdown(void) {
    if (global_order_book) {
        LOG_DEBUG("Destroying order book for symbol: %s", global_symbol);
        order_book_destroy(global_order_book);
        global_order_book = NULL;
    }
    memset(global_symbol, 0, sizeof(global_symbol));
    LOG_INFO("Order handler shutdown complete");
}

bool order_handler_create_book(const char* symbol) {
    // Require symbol to be provided
    if (!symbol || symbol[0] == '\0') {
        LOG_ERROR("Symbol must be provided when creating order book");
        return false;
    }

    // If we already have a book for this symbol, just return success
    if (global_order_book && strcmp(global_symbol, symbol) == 0) {
        LOG_DEBUG("Using existing order book for symbol: %s", symbol);
        return true;
    }

    // Only create a new book if it's a different symbol
    if (!global_order_book || strcmp(global_symbol, symbol) != 0) {
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
        LOG_INFO("Order book created for symbol: %s", global_symbol);
    }

    return true;
}

OrderHandlingResult order_handler_add_order(const Order* order) {
    LOG_DEBUG("Attempting to add order: symbol=%s, price=%.2f, quantity=%u, is_buy=%d",
             order ? order->symbol : "NULL",
             order ? order->price : 0.0,
             order ? order->quantity : 0,
             order ? order->is_buy : -1);

    if (!global_order_book || !order) {
        LOG_ERROR("Invalid order book (%p) or order (%p)", 
                  (void*)global_order_book, (void*)order);
        return ORDER_INVALID;
    }

    // Log current book state
    LOG_DEBUG("Current order book symbol: %s", global_symbol);

    // Validate symbol match
    if (strcmp(global_symbol, order->symbol) != 0) {
        LOG_ERROR("Symbol mismatch. Current book symbol: '%s', Order symbol: '%s'", 
                  global_symbol, order->symbol);
        return ORDER_INVALID;
    }

    // Validate price and quantity
    if (order->price <= 0.0 || order->quantity == 0) {
        LOG_ERROR("Invalid price (%.2f) or quantity (%u)", 
                  order->price, order->quantity);
        return ORDER_INVALID;
    }

    // Attempt to add order
    if (order_book_add(global_order_book, order)) {
        LOG_INFO("Order added successfully: id=%lu, price=%.2f, quantity=%u, is_buy=%d", 
                 order->id, order->price, order->quantity, order->is_buy);
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
