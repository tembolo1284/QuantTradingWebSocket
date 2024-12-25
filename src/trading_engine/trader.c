#include "trading_engine/trader.h"
#include "trading_engine/order.h"
#include "trading_engine/order_book.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

Trader* trader_create(const char* trader_id, const char* name, double balance) {
    if (!trader_id || !name) {
        LOG_ERROR("Attempted to create trader with NULL id or name");
        return NULL;
    }

    if (balance < 0) {
        LOG_ERROR("Attempted to create trader with negative balance: %.2f", balance);
        return NULL;
    }

    Trader* trader = (Trader*)malloc(sizeof(Trader));
    if (!trader) {
        LOG_ERROR("Failed to allocate memory for trader");
        return NULL;
    }

    if (strlen(trader_id) >= MAX_TRADER_ID_LENGTH || 
        strlen(name) >= MAX_TRADER_NAME_LENGTH) {
        LOG_ERROR("Trader ID or name exceeds maximum length");
        free(trader);
        return NULL;
    }

    strncpy(trader->trader_id, trader_id, MAX_TRADER_ID_LENGTH - 1);
    trader->trader_id[MAX_TRADER_ID_LENGTH - 1] = '\0';
    
    strncpy(trader->name, name, MAX_TRADER_NAME_LENGTH - 1);
    trader->name[MAX_TRADER_NAME_LENGTH - 1] = '\0';
    
    trader->balance = balance;

    LOG_INFO("Created new trader: ID=%s, Name=%s, Initial Balance=%.2f",
             trader_id, name, balance);
    return trader;
}

void trader_destroy(Trader* trader) {
    if (trader) {
        LOG_INFO("Destroying trader: ID=%s", trader->trader_id);
        free(trader);
    }
}

const char* trader_get_id(const Trader* trader) {
    if (!trader) {
        LOG_ERROR("Attempted to get ID from NULL trader");
        return NULL;
    }
    return trader->trader_id;
}

const char* trader_get_name(const Trader* trader) {
    if (!trader) {
        LOG_ERROR("Attempted to get name from NULL trader");
        return NULL;
    }
    return trader->name;
}

double trader_get_balance(const Trader* trader) {
    if (!trader) {
        LOG_ERROR("Attempted to get balance from NULL trader");
        return 0.0;
    }
    return trader->balance;
}

void trader_set_id(Trader* trader, const char* trader_id) {
    if (!trader || !trader_id) {
        LOG_ERROR("Invalid parameters for setting trader ID");
        return;
    }

    if (strlen(trader_id) >= MAX_TRADER_ID_LENGTH) {
        LOG_ERROR("New trader ID exceeds maximum length");
        return;
    }

    LOG_INFO("Updating trader ID: %s -> %s", trader->trader_id, trader_id);
    strncpy(trader->trader_id, trader_id, MAX_TRADER_ID_LENGTH - 1);
    trader->trader_id[MAX_TRADER_ID_LENGTH - 1] = '\0';
}

void trader_set_name(Trader* trader, const char* name) {
    if (!trader || !name) {
        LOG_ERROR("Invalid parameters for setting trader name");
        return;
    }

    if (strlen(name) >= MAX_TRADER_NAME_LENGTH) {
        LOG_ERROR("New trader name exceeds maximum length");
        return;
    }

    LOG_INFO("Updating trader name: %s -> %s", trader->name, name);
    strncpy(trader->name, name, MAX_TRADER_NAME_LENGTH - 1);
    trader->name[MAX_TRADER_NAME_LENGTH - 1] = '\0';
}

void trader_set_balance(Trader* trader, double balance) {
    if (!trader) {
        LOG_ERROR("Attempted to set balance for NULL trader");
        return;
    }

    if (balance < 0) {
        LOG_ERROR("Attempted to set negative balance (%.2f) for trader %s",
                 balance, trader->trader_id);
        return;
    }

    LOG_INFO("Setting balance for trader %s: %.2f -> %.2f",
             trader->trader_id, trader->balance, balance);
    trader->balance = balance;
}

int trader_place_order(Trader* trader, OrderBook* order_book, const Order* order) {
    if (!trader || !order_book || !order) {
        LOG_ERROR("Invalid parameters for placing order");
        return -1;
    }

    if (strcmp(trader->trader_id, order_get_trader_id(order)) != 0) {
        LOG_ERROR("Trader ID mismatch: %s attempting to place order for trader %s",
                 trader->trader_id, order_get_trader_id(order));
        return -1;
    }

    // For buy orders, verify sufficient balance
    if (order_is_buy_order(order)) {
        double required_funds = order_get_price(order) * order_get_quantity(order);
        if (required_funds > trader->balance) {
            LOG_ERROR("Insufficient funds for trader %s: required=%.2f, available=%.2f",
                     trader->trader_id, required_funds, trader->balance);
            return -1;
        }
        
        // Reserve the funds
        LOG_INFO("Reserving %.2f from trader %s balance for buy order",
                 required_funds, trader->trader_id);
        trader->balance -= required_funds;
    }

    LOG_INFO("Trader %s placing %s order: Symbol=%s, Price=%.2f, Quantity=%d",
             trader->trader_id,
             order_is_buy_order(order) ? "buy" : "sell",
             order_get_stock_symbol(order),
             order_get_price(order),
             order_get_quantity(order));

    return order_book_add_order(order_book, (Order*)order);
}

void trader_update_balance(Trader* trader, double amount) {
    if (!trader) {
        LOG_ERROR("Attempted to update balance for NULL trader");
        return;
    }

    double new_balance = trader->balance + amount;
    if (new_balance < 0) {
        LOG_ERROR("Balance update would result in negative balance for trader %s",
                 trader->trader_id);
        return;
    }

    LOG_INFO("Updating balance for trader %s: %.2f %s %.2f = %.2f",
             trader->trader_id,
             trader->balance,
             amount >= 0 ? "+" : "-",
             fabs(amount),
             new_balance);

    trader->balance = new_balance;
}
