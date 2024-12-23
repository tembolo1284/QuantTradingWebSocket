#include "trading/engine/order.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

static uint64_t next_order_id = 1;

Order* order_create(const char* symbol, double price, uint32_t quantity, bool is_buy) {
    // Check for NULL or empty symbol
    if (!symbol || symbol[0] == '\0') {
        LOG_ERROR("Empty or NULL symbol");
        return NULL;
    }

    // Check for invalid price
    if (price <= 0.0) {
        LOG_ERROR("Invalid order price: %.2f", price);
        return NULL;
    }

    // Check for zero quantity
    if (quantity <= 0) {
        LOG_ERROR("Invalid order quantity: 0");
        return NULL;
    }

    Order* order = malloc(sizeof(Order));
    if (!order) {
        LOG_ERROR("Failed to allocate memory for order");
        return NULL;
    }

    // Generate unique order ID atomically
    order->id = __atomic_fetch_add(&next_order_id, 1, __ATOMIC_SEQ_CST);
    order->price = price;
    order->quantity = quantity;
    order->timestamp = get_timestamp();
    order->is_buy = is_buy;
    
    // Copy symbol with bounds checking
    strncpy(order->symbol, symbol, sizeof(order->symbol) - 1);
    order->symbol[sizeof(order->symbol) - 1] = '\0';

    LOG_DEBUG("Created order: id=%lu, symbol=%s, price=%.2f, quantity=%u, is_buy=%d",
             order->id, order->symbol, order->price, order->quantity, order->is_buy);

    return order;
}

bool order_validate(const Order* order) {
    if (!order) {
        LOG_ERROR("Null order pointer");
        return false;
    }

    // Basic validation checks
    if (order->price <= 0) {
        LOG_ERROR("Invalid order price: %.2f", order->price);
        return false;
    }

    if (order->quantity == 0) {
        LOG_ERROR("Invalid order quantity: %u", order->quantity);
        return false;
    }

    if (order->symbol[0] == '\0') {
        LOG_ERROR("Empty order symbol");
        return false;
    }

    // Timestamp validation - ensure it's not in the future
    timestamp_t current_time = get_timestamp();
    if (order->timestamp > current_time) {
        LOG_ERROR("Order timestamp is in the future");
        return false;
    }

    return true;
}
