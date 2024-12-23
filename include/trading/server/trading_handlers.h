#ifndef TRADING_HANDLERS_H
#define TRADING_HANDLERS_H

#include "net/websocket_server.h"

typedef struct TradingContext TradingContext;

// Initialize trading handlers
TradingContext* trading_handlers_init(WebSocketServer* server);

// Handle incoming trading messages
void handle_trading_message(WebSocketClient* client, const uint8_t* data, size_t len);

// Cleanup trading handlers
void trading_handlers_cleanup(TradingContext* context);

#endif // TRADING_HANDLERS_H
