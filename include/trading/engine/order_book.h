// include/trading/engine/order_book.h
#ifndef QUANT_TRADING_ORDER_BOOK_H
#define QUANT_TRADING_ORDER_BOOK_H

#include "trading/engine/order.h"

typedef struct OrderNode OrderNode;
typedef struct PriceNode PriceNode;

typedef struct {
    PriceNode* buy_tree;    // AVL tree for buy orders
    PriceNode* sell_tree;   // AVL tree for sell orders
    uint64_t total_orders;
    char symbol[16];
} OrderBook;

// Create new order book
OrderBook* order_book_create(const char* symbol);

// Add order to book
bool order_book_add(OrderBook* book, const Order* order);

// Cancel order
bool order_book_cancel(OrderBook* book, uint64_t order_id);

// Modify order
bool order_book_modify(OrderBook* book, uint64_t order_id, const Order* new_order);

// Get best bid/ask
double order_book_get_best_bid(const OrderBook* book);
double order_book_get_best_ask(const OrderBook* book);

// Free order book
void order_book_destroy(OrderBook* book);

#endif // QUANT_TRADING_ORDER_BOOK_H
