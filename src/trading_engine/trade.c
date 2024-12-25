#include "trading_engine/trade.h"
#include "trading_engine/trader.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Trade* trade_create(const char* buy_order_id,
                   const char* sell_order_id,
                   double trade_price,
                   int trade_quantity) {
    
    if (!buy_order_id || !sell_order_id) {
        LOG_ERROR("Attempted to create trade with NULL order ID(s)");
        return NULL;
    }

    if (trade_price <= 0) {
        LOG_ERROR("Invalid trade price: %.2f", trade_price);
        return NULL;
    }

    if (trade_quantity <= 0) {
        LOG_ERROR("Invalid trade quantity: %d", trade_quantity);
        return NULL;
    }

    Trade* trade = (Trade*)malloc(sizeof(Trade));
    if (!trade) {
        LOG_ERROR("Failed to allocate memory for trade");
        return NULL;
    }

    if (strlen(buy_order_id) >= MAX_ORDER_ID_LENGTH || 
        strlen(sell_order_id) >= MAX_ORDER_ID_LENGTH) {
        LOG_ERROR("Order ID exceeds maximum length");
        free(trade);
        return NULL;
    }

    strncpy(trade->buy_order_id, buy_order_id, MAX_ORDER_ID_LENGTH - 1);
    trade->buy_order_id[MAX_ORDER_ID_LENGTH - 1] = '\0';
    
    strncpy(trade->sell_order_id, sell_order_id, MAX_ORDER_ID_LENGTH - 1);
    trade->sell_order_id[MAX_ORDER_ID_LENGTH - 1] = '\0';
    
    trade->trade_price = trade_price;
    trade->trade_quantity = trade_quantity;

    LOG_INFO("Created new trade: Buy Order=%s, Sell Order=%s, Price=%.2f, Quantity=%d",
             buy_order_id, sell_order_id, trade_price, trade_quantity);
    
    return trade;
}

void trade_destroy(Trade* trade) {
    if (trade) {
        LOG_DEBUG("Destroying trade between buy order %s and sell order %s",
                 trade->buy_order_id, trade->sell_order_id);
        free(trade);
    }
}

const char* trade_get_buy_order_id(const Trade* trade) {
    if (!trade) {
        LOG_ERROR("Attempted to get buy order ID from NULL trade");
        return NULL;
    }
    return trade->buy_order_id;
}

const char* trade_get_sell_order_id(const Trade* trade) {
    if (!trade) {
        LOG_ERROR("Attempted to get sell order ID from NULL trade");
        return NULL;
    }
    return trade->sell_order_id;
}

double trade_get_price(const Trade* trade) {
    if (!trade) {
        LOG_ERROR("Attempted to get price from NULL trade");
        return 0.0;
    }
    return trade->trade_price;
}

int trade_get_quantity(const Trade* trade) {
    if (!trade) {
        LOG_ERROR("Attempted to get quantity from NULL trade");
        return 0;
    }
    return trade->trade_quantity;
}

int trade_execute(Trade* trade, Trader* buyer, Trader* seller) {
    if (!trade || !buyer || !seller) {
        LOG_ERROR("Invalid parameters for trade execution");
        return -1;
    }

    double total_amount = trade->trade_price * trade->trade_quantity;

    LOG_INFO("Executing trade: Buy Order=%s, Sell Order=%s, Price=%.2f, Quantity=%d, Total=%.2f",
             trade->buy_order_id, trade->sell_order_id,
             trade->trade_price, trade->trade_quantity, total_amount);

    // Update seller's balance
    LOG_DEBUG("Crediting seller %s with %.2f", 
             trader_get_id(seller), total_amount);
    trader_update_balance(seller, total_amount);

    // Note: Buyer's balance was already reduced when placing the order
    // If the execution price is different from the original order price,
    // we might need to refund the difference here

    LOG_INFO("Trade execution completed successfully");
    return 0;
}

char* trade_to_string(const Trade* trade) {
    if (!trade) {
        LOG_ERROR("Attempted to convert NULL trade to string");
        return NULL;
    }

    char* str = (char*)malloc(256);
    if (!str) {
        LOG_ERROR("Failed to allocate memory for trade string representation");
        return NULL;
    }

    snprintf(str, 256,
             "Trade{buy_order=%s, sell_order=%s, price=%.2f, quantity=%d}",
             trade->buy_order_id,
             trade->sell_order_id,
             trade->trade_price,
             trade->trade_quantity);

    LOG_DEBUG("Created string representation for trade between %s and %s",
             trade->buy_order_id, trade->sell_order_id);
    
    return str;
}
