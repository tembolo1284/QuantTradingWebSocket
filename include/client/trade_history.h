#ifndef CLIENT_TRADE_HISTORY_H
#define CLIENT_TRADE_HISTORY_H

#include "protocol/message_types.h"

typedef struct TradeHistory TradeHistory;

typedef struct {
    int max_trades;
    bool record_all_trades;  // Record other traders' trades too
} TradeHistoryConfig;

TradeHistory* trade_history_create(const TradeHistoryConfig* config);
void trade_history_destroy(TradeHistory* history);

int trade_history_add_trade(TradeHistory* history, const TradeMessage* trade);
int trade_history_get_trades(TradeHistory* history, TradeMessage* trades, 
                           int max_trades, int* num_trades);

// Statistics
double trade_history_get_avg_price(TradeHistory* history, const char* symbol);
int trade_history_get_volume(TradeHistory* history, const char* symbol);
double trade_history_get_vwap(TradeHistory* history, const char* symbol);

#endif /* CLIENT_TRADE_HISTORY_H */
