#ifndef MESSAGES_H
#define MESSAGES_H

#include "trading/engine/order_book.h"
#include "trading/engine/trade.h"
#include <stdint.h>

typedef enum {
    MESSAGE_UNKNOWN,
    MESSAGE_ORDER_ADD,
    MESSAGE_ORDER_CANCEL,
    MESSAGE_BOOK_QUERY,
    MESSAGE_TRADE
} MessageType;

typedef struct {
    MessageType type;
    char symbol[16];
    double price;
    uint32_t quantity;
    bool is_buy;
} OrderAddMessage;

typedef struct {
    MessageType type;
    uint64_t order_id;
} OrderCancelMessage;

typedef struct {
    BookQueryType type;
    char symbol[16];
} BookQueryConfig;

// Parse JSON message into appropriate struct
MessageType parse_message(const char* json, void* out_message);

// Serialize responses
char* order_response_serialize(uint64_t order_id, bool success, const char* message);
char* cancel_response_serialize(CancelResult result, uint64_t order_id);
char* trade_notification_serialize(const Trade* trade);
char* book_query_serialize(const BookQueryConfig* config);

#endif // MESSAGES_H
