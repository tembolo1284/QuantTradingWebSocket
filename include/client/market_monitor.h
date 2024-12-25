#ifndef CLIENT_MARKET_MONITOR_H
#define CLIENT_MARKET_MONITOR_H

#include "protocol/message_types.h"
#include <stdbool.h>

typedef struct MarketMonitor MarketMonitor;

typedef struct {
    int max_symbols;
    int update_interval_ms;
    bool display_full_depth;
} MarketMonitorConfig;

MarketMonitor* market_monitor_create(const MarketMonitorConfig* config);
void market_monitor_destroy(MarketMonitor* monitor);

int market_monitor_subscribe(MarketMonitor* monitor, const char* symbol);
int market_monitor_unsubscribe(MarketMonitor* monitor, const char* symbol);

int market_monitor_update_book(MarketMonitor* monitor, const BookSnapshot* snapshot);
int market_monitor_update_trade(MarketMonitor* monitor, const TradeMessage* trade);

void market_monitor_display(MarketMonitor* monitor);
void market_monitor_clear(MarketMonitor* monitor);

#endif /* CLIENT_MARKET_MONITOR_H */
