#ifndef PROTOCOL_VALIDATION_H
#define PROTOCOL_VALIDATION_H

#include "protocol/message_types.h"
#include <stdbool.h>
#include <stddef.h>

// Message validation
bool validate_order_message(const OrderMessage* order, char* error_msg, size_t error_size);
bool validate_trade_message(const TradeMessage* trade, char* error_msg, size_t error_size);
bool validate_book_snapshot(const BookSnapshot* snapshot, char* error_msg, size_t error_size);
bool validate_server_status(const ServerStatus* status, char* error_msg, size_t error_size);

// Field validation
bool validate_symbol(const char* symbol);
bool validate_order_id(const char* order_id);
bool validate_trader_id(const char* trader_id);
bool validate_price(double price);
bool validate_quantity(int quantity);

#endif /* PROTOCOL_VALIDATION_H */
