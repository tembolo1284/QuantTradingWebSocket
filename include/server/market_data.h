#ifndef SERVER_MARKET_DATA_H
#define SERVER_MARKET_DATA_H

#include "trading_engine/order_book.h"
#include "protocol/message_types.h"
#include <pthread.h>

typedef struct MarketData MarketData;

typedef struct {
    int snapshot_interval_ms;
    int max_depth;
    int max_symbols;
} MarketDataConfig;

MarketData* market_data_create(const MarketDataConfig* config);
void market_data_destroy(MarketData* market);

// Snapshot generation
int market_data_get_snapshot(MarketData* market, const char* symbol, BookSnapshot* snapshot);
int market_data_get_all_snapshots(MarketData* market, BookSnapshot** snapshots, int* num_snapshots);

// Order book updates
int market_data_update_book(MarketData* market, const char* symbol, const OrderBook* book);
int market_data_remove_book(MarketData* market, const char* symbol);

// Market statistics
int market_data_get_symbol_count(const MarketData* market);
int market_data_get_total_orders(const MarketData* market);
double market_data_get_total_volume(const MarketData* market);

// Timer control
int market_data_start_snapshot_timer(MarketData* market);
int market_data_stop_snapshot_timer(MarketData* market);

#endif /* SERVER_MARKET_DATA_H */
