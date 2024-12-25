#ifndef TRADE_H
#define TRADE_H

#include <stddef.h>

#define MAX_ORDER_ID_LENGTH 64

// Forward declarations
struct Trader;

typedef struct Trade {
    char buy_order_id[MAX_ORDER_ID_LENGTH];
    char sell_order_id[MAX_ORDER_ID_LENGTH];
    double trade_price;
    int trade_quantity;
} Trade;

// Constructor and destructor
Trade* trade_create(const char* buy_order_id,
                   const char* sell_order_id,
                   double trade_price,
                   int trade_quantity);
void trade_destroy(Trade* trade);

// Getters
const char* trade_get_buy_order_id(const Trade* trade);
const char* trade_get_sell_order_id(const Trade* trade);
double trade_get_price(const Trade* trade);
int trade_get_quantity(const Trade* trade);

// Execution
int trade_execute(Trade* trade, struct Trader* buyer, struct Trader* seller);

// String representation
char* trade_to_string(const Trade* trade);

#endif /* TRADE_H */
