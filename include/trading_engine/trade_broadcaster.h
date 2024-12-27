#ifndef TRADING_ENGINE_TRADE_BROADCASTER_H
#define TRADING_ENGINE_TRADE_BROADCASTER_H

#include <stdint.h>
#include <time.h>
#include "server/ws_server.h"

typedef struct TradeBroadcaster TradeBroadcaster;

TradeBroadcaster* trade_broadcaster_create(WSServer* server);
void trade_broadcaster_destroy(TradeBroadcaster* broadcaster);

void trade_broadcaster_send_trade(TradeBroadcaster* broadcaster,
                               const char* symbol,
                               const char* buy_order_id,
                               const char* sell_order_id, 
                               double price,
                               int quantity,
                               time_t timestamp);

#endif /* TRADING_ENGINE_TRADE_BROADCASTER_H */
