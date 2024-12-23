#ifndef ORDER_H
#define ORDER_H

#include "common/types.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct Order {
    uint64_t id;              // Unique order identifier
    char symbol[16];          // Trading symbol (e.g., "AAPL")
    double price;             // Order price
    uint32_t quantity;        // Order quantity
    timestamp_t timestamp;    // Order creation timestamp
    bool is_buy;             // true for buy orders, false for sell orders
} Order;

// Create a new order
Order* order_create(const char* symbol, double price, uint32_t quantity, bool is_buy);

// Validate an order's fields
bool order_validate(const Order* order);

#endif // ORDER_H
