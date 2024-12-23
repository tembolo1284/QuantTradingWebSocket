#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include "order.h"
#include "trade.h"
#include <stdbool.h>
#include <stddef.h>

// Forward declarations for internal structures
typedef struct OrderNode OrderNode;
typedef struct PriceNode PriceNode;

// Main order book structure
typedef struct OrderBook {
    PriceNode* buy_tree;            // AVL tree for buy orders
    PriceNode* sell_tree;           // AVL tree for sell orders
    size_t total_orders;            // Total number of active orders
    char symbol[16];                // Trading symbol
    TradeCallback trade_callback;    // Callback for trade notifications
    void* callback_user_data;       // User data for callback
} OrderBook;

// Callback type for trade notifications
typedef void (*TradeCallback)(const Trade* trade, void* user_data);

// Cancel operation result codes
typedef enum CancelResult {
    CANCEL_SUCCESS,           // Order was found and cancelled
    CANCEL_ORDER_NOT_FOUND,   // Order ID doesn't exist
    CANCEL_INVALID_BOOK,      // Order book is NULL or invalid
    CANCEL_ALREADY_FILLED     // Order was already fully filled/executed
} CancelResult;

// Create a new order book for a symbol
OrderBook* order_book_create(const char* symbol);

// Add an order to the book (will attempt matching first)
bool order_book_add(OrderBook* book, const Order* order);

// Cancel an existing order
CancelResult order_book_cancel(OrderBook* book, uint64_t order_id);

// Get best bid/ask prices
double order_book_get_best_bid(const OrderBook* book);
double order_book_get_best_ask(const OrderBook* book);

// Set callback for trade notifications
void order_book_set_trade_callback(OrderBook* book, 
                                 TradeCallback callback, 
                                 void* user_data);

// Clean up an order book
void order_book_destroy(OrderBook* book);

// Get the symbol for an order book
const char* order_book_get_symbol(const OrderBook* book);

// Get total number of orders in the book
size_t order_book_get_order_count(const OrderBook* book);

#endif // ORDER_BOOK_H
