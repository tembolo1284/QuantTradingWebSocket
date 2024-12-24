#ifndef CLIENT_HELPER_H
#define CLIENT_HELPER_H

#include "net/websocket.h"
#include "trading/protocol/messages.h"
#include "utils/json_utils.h"

// Function to generate unique order ID
uint64_t generate_order_id(void);

// Function to print order book for a specific symbol
void print_order_book(const BookSymbol* symbol);

// Function to send book query
bool send_book_query(WebSocket* ws, const char* symbol);

// Function to send order
bool send_order(WebSocket* ws, bool is_buy, double price, uint32_t quantity, const char* symbol);

// Function to send order cancellation
void send_order_cancel(WebSocket* ws, uint64_t order_id);

// Function to print available commands/usage
void print_usage(void);

#endif // CLIENT_HELPER_H
