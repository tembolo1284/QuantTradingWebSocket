#ifndef PROTOCOL_MESSAGE_TYPES_H
#define PROTOCOL_MESSAGE_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Client -> Server messages
typedef enum {
    MSG_PLACE_ORDER = 1,
    MSG_CANCEL_ORDER = 2,
    MSG_REQUEST_BOOK = 3,
    MSG_SUBSCRIBE_SYMBOL = 4,
    MSG_UNSUBSCRIBE_SYMBOL = 5
} ClientMessageType;

// Server -> Client messages
typedef enum {
    MSG_ORDER_ACCEPTED = 101,
    MSG_ORDER_REJECTED = 102,
    MSG_ORDER_CANCELED = 103,
    MSG_TRADE_EXECUTED = 104,
    MSG_BOOK_SNAPSHOT = 105,
    MSG_SERVER_STATUS = 106,
    MSG_ERROR = 107
} ServerMessageType;

// Message structure for order placement
typedef struct {
    char symbol[16];
    char order_id[32];
    char trader_id[32];
    double price;
    int quantity;
    bool is_buy;
} OrderMessage;

// Message structure for trade execution
typedef struct {
    char symbol[16];
    char buy_order_id[32];
    char sell_order_id[32];
    double price;
    int quantity;
    int64_t timestamp;
} TradeMessage;

// Message structure for order book snapshot
typedef struct {
    char symbol[16];
    int num_bids;
    int num_asks;
    size_t max_orders;
    // Arrays will be allocated dynamically
    double* bid_prices;
    int* bid_quantities;
    double* ask_prices;
    int* ask_quantities;
} BookSnapshot;

// Message structure for server status
typedef struct {
    bool is_ready;
    int num_connected_clients;
    int num_active_orders;
    int64_t timestamp;
} ServerStatus;

#endif /* PROTOCOL_MESSAGE_TYPES_H */
