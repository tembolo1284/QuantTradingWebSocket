#ifndef CLIENT_MARKET_VIEW_H
#define CLIENT_MARKET_VIEW_H

#include "protocol/message_types.h"

typedef struct MarketView MarketView;

typedef struct {
    int update_interval_ms;
    int max_depth;
    bool show_trades;
} MarketViewConfig;

MarketView* market_view_create(const MarketViewConfig* config);
void market_view_destroy(MarketView* view);

// Display updates
int market_view_update_book(MarketView* view, const BookSnapshot* snapshot);
int market_view_show_trade(MarketView* view, const TradeMessage* trade);
int market_view_show_status(MarketView* view, const ServerStatus* status);

// View control
int market_view_clear(MarketView* view);
int market_view_refresh(MarketView* view);
int market_view_set_symbol(MarketView* view, const char* symbol);

#endif /* CLIENT_MARKET_VIEW_H */
