#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include "avl_tree.h"
#include <stdbool.h>

typedef struct OrderBook {
    AVLTree* buy_orders;
    AVLTree* sell_orders;
} OrderBook;

// Constructor and destructor
OrderBook* order_book_create(void);
void order_book_destroy(OrderBook* book);

// Order operations
int order_book_add_order(OrderBook* book, struct Order* order);
void order_book_match_orders(OrderBook* book);
int order_book_cancel_order(OrderBook* book, const char* order_id, bool is_buy_order);

// Query operations
int order_book_get_quantity_at_price(const OrderBook* book, double price, bool is_buy_order);
bool order_book_is_order_canceled(const OrderBook* book, const char* order_id, bool is_buy_order);

// Traversal callbacks
typedef void (*OrderCallback)(struct Order* order, void* user_data);
void order_book_traverse_buy_orders(const OrderBook* book, OrderCallback callback, void* user_data);
void order_book_traverse_sell_orders(const OrderBook* book, OrderCallback callback, void* user_data);

#endif /* ORDER_BOOK_H */
