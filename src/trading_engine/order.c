#include "trading_engine/order.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

Order* order_create(const char* order_id,
                   const char* trader_id,
                   const char* stock_symbol,
                   double price,
                   int quantity,
                   bool is_buy_order) {
    
    Order* order = (Order*)malloc(sizeof(Order));
    if (!order) {
        LOG_ERROR("Failed to allocate memory for order");
        return NULL;
    }

    if (strlen(order_id) >= MAX_ID_LENGTH || 
        strlen(trader_id) >= MAX_ID_LENGTH || 
        strlen(stock_symbol) >= MAX_SYMBOL_LENGTH) {
        LOG_ERROR("Input string length exceeds maximum allowed length");
        free(order);
        return NULL;
    }

    strncpy(order->order_id, order_id, MAX_ID_LENGTH - 1);
    order->order_id[MAX_ID_LENGTH - 1] = '\0';
    
    strncpy(order->trader_id, trader_id, MAX_ID_LENGTH - 1);
    order->trader_id[MAX_ID_LENGTH - 1] = '\0';
    
    strncpy(order->stock_symbol, stock_symbol, MAX_SYMBOL_LENGTH - 1);
    order->stock_symbol[MAX_SYMBOL_LENGTH - 1] = '\0';

    order->price = price;
    order->quantity = quantity;
    order->remaining_quantity = quantity;
    order->is_buy_order = is_buy_order;
    order->timestamp = (int64_t)time(NULL);
    order->is_canceled = false;

    LOG_INFO("Created new %s order: ID=%s, Symbol=%s, Price=%.2f, Quantity=%d",
             is_buy_order ? "buy" : "sell", order_id, stock_symbol, price, quantity);

    return order;
}

void order_destroy(Order* order) {
    if (order) {
        LOG_DEBUG("Destroying order: ID=%s", order->order_id);
        memset(order, 0, sizeof(Order)); //clear potentially sensitive data.
        free(order);
    }
}

const char* order_get_id(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to get ID from NULL order");
        return NULL;
    }
    return order->order_id;
}

const char* order_get_trader_id(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to get trader ID from NULL order");
        return NULL;
    }
    return order->trader_id;
}

const char* order_get_stock_symbol(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to get stock symbol from NULL order");
        return NULL;
    }
    return order->stock_symbol;
}

double order_get_price(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to get price from NULL order");
        return 0.0;
    }
    return order->price;
}

int order_get_quantity(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to get quantity from NULL order");
        return 0;
    }
    return order->quantity;
}

int order_get_remaining_quantity(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to get remaining quantity from NULL order");
        return 0;
    }
    return order->remaining_quantity;
}

bool order_is_buy_order(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to check buy/sell status of NULL order");
        return false;
    }
    return order->is_buy_order;
}

int64_t order_get_timestamp(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to get timestamp from NULL order");
        return 0;
    }
    return order->timestamp;
}

bool order_is_canceled(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to check canceled status of NULL order");
        return false;
    }
    return order->is_canceled;
}

void order_set_price(Order* order, double new_price) {
    if (!order) {
        LOG_ERROR("Attempted to set price on NULL order");
        return;
    }
    if (new_price <= 0.0) {
        LOG_ERROR("Attempted to set invalid price (%.2f) on order %s", 
                 new_price, order->order_id);
        return;
    }
    LOG_INFO("Updating order %s price: %.2f -> %.2f", 
             order->order_id, order->price, new_price);
    order->price = new_price;
}

int order_set_quantity(Order* order, int new_quantity) {
    if (!order) {
        LOG_ERROR("Attempted to set quantity on NULL order");
        return -1;
    }
    if (new_quantity < 0) {
        LOG_ERROR("Attempted to set negative quantity (%d) on order %s", 
                 new_quantity, order->order_id);
        return -1;
    }
    
    LOG_INFO("Updating order %s quantity: %d -> %d", 
             order->order_id, order->quantity, new_quantity);
    order->quantity = new_quantity;
    order->remaining_quantity = new_quantity;
    return 0;
}

int order_reduce_quantity(Order* order, int amount) {
    if (!order) {
        LOG_ERROR("Attempted to reduce quantity on NULL order");
        return -1;
    }
    if (amount < 0) {
        LOG_ERROR("Attempted to reduce quantity by negative amount (%d) on order %s", 
                 amount, order->order_id);
        return -1;
    }
    if (amount > order->remaining_quantity) {
        LOG_ERROR("Attempted to reduce quantity by %d when only %d remaining for order %s",
                 amount, order->remaining_quantity, order->order_id);
        return -1;
    }

    LOG_INFO("Reducing order %s remaining quantity: %d - %d = %d",
             order->order_id, order->remaining_quantity, amount,
             order->remaining_quantity - amount);
    
    order->remaining_quantity -= amount;
    return 0;
}

void order_cancel(Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to cancel NULL order");
        return;
    }
    if (order->is_canceled) {
        LOG_WARN("Attempted to cancel already canceled order %s", order->order_id);
        return;
    }
    
    LOG_INFO("Canceling order %s", order->order_id);
    order->is_canceled = true;
}

bool order_equals(const Order* order1, const Order* order2) {
    if (!order1 || !order2) {
        LOG_ERROR("Attempted to compare NULL order(s)");
        return false;
    }
    
    bool equals = strcmp(order1->order_id, order2->order_id) == 0;
    LOG_DEBUG("Comparing orders %s and %s: %s",
             order1->order_id, order2->order_id,
             equals ? "equal" : "not equal");
    return equals;
}

int order_compare(const Order* order1, const Order* order2) {
    if (!order1 || !order2) {
        LOG_ERROR("Attempted to compare NULL order(s)");
        return 0;
    }

    // First compare by price
    if (order1->price != order2->price) {
        if (order1->is_buy_order) {
            return order1->price > order2->price ? -1 : 1;  // Higher price first for buy orders
        } else {
            return order1->price < order2->price ? -1 : 1;  // Lower price first for sell orders
        }
    }

    // If prices are equal, compare by timestamp
    if (order1->timestamp != order2->timestamp) {
        return order1->timestamp < order2->timestamp ? -1 : 1;
    }

    LOG_DEBUG("Comparing orders %s and %s: equal priority",
             order1->order_id, order2->order_id);
    return 0;
}

char* order_to_string(const Order* order) {
    if (!order) {
        LOG_ERROR("Attempted to convert NULL order to string");
        return NULL;
    }

    // Allocate enough space for the string representation
    char* str = (char*)malloc(256);
    if (!str) {
        LOG_ERROR("Failed to allocate memory for order string representation");
        return NULL;
    }

    snprintf(str, 256,
             "Order{id=%s, trader=%s, symbol=%s, price=%.2f, qty=%d, remaining=%d, %s, %s}",
             order->order_id,
             order->trader_id,
             order->stock_symbol,
             order->price,
             order->quantity,
             order->remaining_quantity,
             order->is_buy_order ? "BUY" : "SELL",
             order->is_canceled ? "CANCELED" : "ACTIVE");

    LOG_DEBUG("Created string representation for order %s", order->order_id);
    return str;
}
