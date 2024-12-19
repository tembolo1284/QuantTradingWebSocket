// include/trading/order.h
#ifndef QUANT_TRADING_ORDER_H
#define QUANT_TRADING_ORDER_H

#include "common/types.h"

typedef struct {
    uint64_t id;
    double price;
    uint32_t quantity;
    timestamp_t timestamp;
    bool is_buy;
    char symbol[16];
} Order;

typedef struct {
    uint64_t id;
    char symbol[16];
    double price;
    uint32_t quantity;
    timestamp_t timestamp;
    bool is_buy;
    bool is_market_order;
    double stop_price;      // For stop orders
    uint64_t expire_time;   // For time-in-force orders
} ExtendedOrder;

// Create new order
Order* order_create(const char* symbol, double price, uint32_t quantity, bool is_buy);

// Validate order
bool order_validate(const Order* order);

#endif // QUANT_TRADING_ORDER_H
