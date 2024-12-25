#ifndef PROTOCOL_JSON_PROTOCOL_H
#define PROTOCOL_JSON_PROTOCOL_H

#include "protocol/message_types.h"
#include <cJSON/cJSON.h>
#include <stdbool.h>

// Message serialization
char* serialize_order_message(const OrderMessage* order);
char* serialize_trade_message(const TradeMessage* trade);
char* serialize_book_snapshot(const BookSnapshot* snapshot);
char* serialize_server_status(const ServerStatus* status);

// Message deserialization
bool parse_order_message(const char* json, OrderMessage* order);
bool parse_trade_message(const char* json, TradeMessage* trade);
bool parse_book_snapshot(const char* json, BookSnapshot* snapshot);
bool parse_server_status(const char* json, ServerStatus* status);

// Helper functions
cJSON* create_base_message(int type);
bool parse_base_message(const char* json, int* type);

// Error handling
const char* get_last_protocol_error(void);

#endif /* PROTOCOL_JSON_PROTOCOL_H */
