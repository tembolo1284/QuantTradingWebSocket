#ifndef QUANT_TRADING_MESSAGES_H
#define QUANT_TRADING_MESSAGES_H

#include "trading/engine/order_book.h"
#include "common/types.h"
#include "net/websocket_server.h"

// Book query result structure
typedef struct {
    BookQueryType type;
    char symbol[16];  // Used when querying a specific symbol
} BookQueryConfig;

// Serialize book query result to JSON
char* book_query_serialize(const BookQueryConfig* config);

#endif // QUANT_TRADING_MESSAGES_H
