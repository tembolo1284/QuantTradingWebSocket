#ifndef TRADING_PROTOCOL_MESSAGES_H
#define TRADING_PROTOCOL_MESSAGES_H

#include "trading/engine/order_book.h"
#include "trading/engine/trade.h"
#include "common/types.h"  // Include this for BookQueryType
#include <stdint.h>
#include <stdbool.h>

// Message types for protocol communication
typedef enum {
    MESSAGE_UNKNOWN,
    MESSAGE_ORDER_ADD,
    MESSAGE_ORDER_CANCEL,
    MESSAGE_BOOK_QUERY,
    MESSAGE_TRADE
} MessageType;

// Message structures
typedef struct {
    MessageType type;
    char symbol[16];      // Fixed size for symbol
    double price;
    uint32_t quantity;
    bool is_buy;
} OrderAddMessage;

typedef struct {
    MessageType type;
    uint64_t order_id;
} OrderCancelMessage;

typedef struct {
    BookQueryType type;   // Using BookQueryType from common/types.h
    char symbol[16];      // Fixed size for symbol
} BookQueryConfig;

/**
 * Parse JSON message into appropriate message structure
 * 
 * @param json The JSON string to parse
 * @param out_message Pointer to structure to fill (must be appropriate type)
 * @return MessageType indicating the type of message parsed
 */
MessageType parse_message(const char* json, void* out_message);

/**
 * Serialize order response into JSON string
 * 
 * @param order_id The ID of the order being responded to
 * @param success Whether the order was successful
 * @param message Optional message (can be NULL)
 * @return Allocated string containing JSON response (must be freed by caller)
 */
char* order_response_serialize(uint64_t order_id, bool success, const char* message);

/**
 * Serialize cancel response into JSON string
 * 
 * @param result The result of the cancel operation
 * @param order_id The ID of the order that was cancelled
 * @return Allocated string containing JSON response (must be freed by caller)
 */
char* cancel_response_serialize(CancelResult result, uint64_t order_id);

/**
 * Serialize trade notification into JSON string
 * 
 * @param trade Pointer to the Trade structure to serialize
 * @return Allocated string containing JSON response (must be freed by caller)
 */
char* trade_notification_serialize(const Trade* trade);

/**
 * Serialize order book query response into JSON string
 * 
 * @param config Pointer to the BookQueryConfig structure containing query parameters
 * @return Allocated string containing JSON response (must be freed by caller)
 */
char* book_query_serialize(const BookQueryConfig* config);

#endif // TRADING_PROTOCOL_MESSAGES_H
