#ifndef QUANT_TRADING_BOOK_QUERY_HANDLER_H
#define QUANT_TRADING_BOOK_QUERY_HANDLER_H

#include "trading/order_book.h"
#include "common.h"
#include "net/websocket_server.h"

// Book query result structure
typedef struct {
    BookQueryType type;
    char symbol[16];  // Used when querying a specific symbol
} BookQueryConfig;

// Serialize book query result to JSON
char* book_query_serialize(const BookQueryConfig* config);

#endif // QUANT_TRADING_BOOK_QUERY_HANDLER_H
