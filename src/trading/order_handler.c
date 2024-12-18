#include "trading/order_handler.h"
#include "utils/json_utils.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

// Static order book to manage orders
static OrderBook* global_order_book = NULL;
static char global_symbol[20] = {0};

bool order_handler_init(void) {
    global_order_book = NULL;
    memset(global_symbol, 0, sizeof(global_symbol));
    return true;
}

void order_handler_shutdown(void) {
    if (global_order_book) {
        order_book_destroy(global_order_book);
        global_order_book = NULL;
    }
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

OrderHandlingResult order_handler_add_order(const Order* order) {
    if (!global_order_book || !order) {
        LOG_ERROR("Invalid order book or order");
        return ORDER_INVALID;
    }

    // Basic order validation
    if (order->quantity == 0) {
        LOG_ERROR("Order quantity cannot be zero");
        return ORDER_INVALID;
    }

    // Validate price
    if (order->price <= 0) {
        LOG_ERROR("Order price must be positive");
        return ORDER_INVALID;
    }

    // Attempt to add order
    if (order_book_add(global_order_book, order)) {
        LOG_INFO("Order added successfully: price=%.2f, quantity=%u, is_buy=%d", 
                 order->price, order->quantity, order->is_buy);
        return ORDER_SUCCESS;
    } else {
        LOG_ERROR("Failed to add order to order book");
        // Let's print out current book state for debugging
        LOG_INFO("Current book state - Best Bid: %.2f, Best Ask: %.2f", 
                 order_book_get_best_bid(global_order_book),
                 order_book_get_best_ask(global_order_book));
        return ORDER_REJECTED;
    }
}

OrderHandlingResult order_handler_cancel_order(uint64_t order_id) {
    if (!global_order_book || !order_id) {
        LOG_ERROR("Invalid order book or order ID");
        return ORDER_INVALID;
    }

    if (order_book_cancel(global_order_book, order_id)) {
        LOG_INFO("Order %lu cancelled successfully", order_id);
        return ORDER_SUCCESS;
    } else {
        LOG_ERROR("Failed to cancel order %lu", order_id);
        return ORDER_REJECTED;
    }
}

OrderBook* order_handler_get_book(void) {
    return global_order_book;
}

char* order_handler_serialize_book(void) {
    if (!global_order_book) {
        LOG_ERROR("No order book available");
        return NULL;
    }

    return json_serialize_order_book(global_order_book);
}
