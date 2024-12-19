#ifndef QUANT_TRADING_JSON_UTILS_H
#define QUANT_TRADING_JSON_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include "trading/engine/order.h"
#include "trading/engine/order_book.h"
#include "common/types.h"
#include <cJSON/cJSON.h>

// Maximum number of orders per price level
#define MAX_ORDERS_PER_PRICE 1000
// Maximum number of symbols in a response
#define MAX_SYMBOLS 10

// JSON message types
typedef enum {
    JSON_MSG_ORDER_ADD,
    JSON_MSG_ORDER_CANCEL,
    JSON_MSG_BOOK_QUERY,
    JSON_MSG_BOOK_RESPONSE,
    JSON_MSG_UNKNOWN
} JsonMessageType;

// Structure for order in book response
typedef struct {
    uint64_t id;
    double price;
    uint32_t quantity;
} BookOrder;

// Structure for symbol-specific order book data
typedef struct {
    char symbol[16];
    BookOrder buy_orders[MAX_ORDERS_PER_PRICE];
    size_t buy_orders_count;
    BookOrder sell_orders[MAX_ORDERS_PER_PRICE];
    size_t sell_orders_count;
    double best_bid;
    double best_ask;
} BookSymbol;

// Structure to represent a parsed message
typedef struct {
    JsonMessageType type;
    union {
        struct {
            char symbol[16];
            Order order;
        } order_add;
        
        struct {
            uint64_t order_id;
        } order_cancel;
        
        struct {
            BookQueryType type;
            char symbol[16];
        } book_query;
        
        struct {
            BookSymbol symbols[MAX_SYMBOLS];
            size_t symbols_count;
        } book_response;
    } data;
} ParsedMessage;

// Parse a JSON string into a structured message
bool json_parse_message(const char* json_str, ParsedMessage* parsed_msg);

// Convert a parsed message back to JSON string
char* json_serialize_message(const ParsedMessage* parsed_msg);

// Convert an order book to a JSON response
char* json_serialize_order_book(const OrderBook* book);

// Free any dynamically allocated JSON resources
void json_free_parsed_message(ParsedMessage* parsed_msg);

#endif // QUANT_TRADING_JSON_UTILS_H
