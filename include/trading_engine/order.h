#ifndef TRADING_ENGINE_ORDER_H
#define TRADING_ENGINE_ORDER_H

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_ID_LENGTH 64
#define MAX_SYMBOL_LENGTH 16

typedef struct Order {
    char order_id[MAX_ID_LENGTH];
    char trader_id[MAX_ID_LENGTH];
    char symbol[MAX_SYMBOL_LENGTH];
    double price;
    int quantity;
    int remaining_quantity;
    bool is_buy_order;
    int64_t timestamp;
    bool is_canceled;
} Order;

// Constructor and destructor
Order* order_create(const char* order_id,
                   const char* trader_id,
                   const char* symbol,
                   double price,
                   int quantity,
                   bool is_buy_order);
void order_destroy(Order* order);

// Getters
const char* order_get_id(const Order* order);
const char* order_get_trader_id(const Order* order);
const char* order_get_stock_symbol(const Order* order);
double order_get_price(const Order* order);
int order_get_quantity(const Order* order);
int order_get_remaining_quantity(const Order* order);
bool order_is_buy_order(const Order* order);
int64_t order_get_timestamp(const Order* order);
bool order_is_canceled(const Order* order);

// Setters
void order_set_price(Order* order, double new_price);
int order_set_quantity(Order* order, int new_quantity);
int order_reduce_quantity(Order* order, int amount);
void order_cancel(Order* order);

// Comparison functions
bool order_equals(const Order* order1, const Order* order2);
int order_compare(const Order* order1, const Order* order2);

// String representation
char* order_to_string(const Order* order);

#endif /* TRADING_ENGINE_ORDER_H */
