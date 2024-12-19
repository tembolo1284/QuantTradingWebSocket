#include "trading/engine/order.h"
#include <stdlib.h>
#include <string.h>

static uint64_t next_order_id = 1;

Order* order_create(const char* symbol, double price, uint32_t quantity, bool is_buy) {
    if (!symbol || price <= 0 || quantity == 0) {
        return NULL;
    }

    Order* order = malloc(sizeof(Order));
    if (!order) {
        return NULL;
    }

    // Initialize order fields
    order->id = __atomic_fetch_add(&next_order_id, 1, __ATOMIC_SEQ_CST);
    order->price = price;
    order->quantity = quantity;
    order->timestamp = get_timestamp();
    order->is_buy = is_buy;
    strncpy(order->symbol, symbol, sizeof(order->symbol) - 1);
    order->symbol[sizeof(order->symbol) - 1] = '\0';

    return order;
}

bool order_validate(const Order* order) {
    if (!order) {
        return false;
    }

    // Basic validation checks
    if (order->price <= 0 || 
        order->quantity == 0 || 
        order->symbol[0] == '\0') {
        return false;
    }

    // Timestamp validation
    timestamp_t current_time = get_timestamp();
    if (order->timestamp > current_time) {
        return false;
    }

    return true;
}
