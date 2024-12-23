#ifndef TRADE_H
#define TRADE_H

#include "order.h"

typedef struct Trade {
    uint64_t id;                 // Unique trade ID
    uint64_t buy_order_id;       // ID of the buy order
    uint64_t sell_order_id;      // ID of the sell order
    char symbol[32];             // Symbol that was traded
    double price;                // Price at which the trade occurred
    uint32_t quantity;           // Quantity that was traded
    timestamp_t timestamp;       // When the trade occurred
} Trade;

// Create a new trade record
Trade* trade_create(const Order* buy_order, const Order* sell_order, uint32_t quantity);

typedef void (*TradeCallback)(const Trade* trade, void* user_data);
#endif
