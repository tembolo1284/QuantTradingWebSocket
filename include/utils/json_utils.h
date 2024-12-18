#ifndef QUANT_TRADING_JSON_UTILS_H
#define QUANT_TRADING_JSON_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include "trading/order.h"
#include "trading/order_book.h"
#include <cJSON/cJSON.h>

// JSON message types
typedef enum {
    JSON_MSG_ORDER_ADD,
    JSON_MSG_ORDER_CANCEL,
    JSON_MSG_BOOK_QUERY,
    JSON_MSG_BOOK_RESPONSE,
    JSON_MSG_UNKNOWN
} JsonMessageType;

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
            char symbol[16];
        } book_query;
        
        struct {
            char symbol[16];
            double best_bid;
            double best_ask;
            // Additional fields could be added for full order book details
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
