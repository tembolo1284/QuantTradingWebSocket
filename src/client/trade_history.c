#include "client/trade_history.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct TradeRecord {
    TradeMessage trade;
    struct TradeRecord* next;
} TradeRecord;

struct TradeHistory {
    TradeRecord* trades;
    int trade_count;
    int max_trades;
    bool record_all_trades;
    pthread_mutex_t lock;
};

TradeHistory* trade_history_create(const TradeHistoryConfig* config) {
    TradeHistory* history = calloc(1, sizeof(TradeHistory));
    if (!history) {
        LOG_ERROR("Failed to allocate trade history");
        return NULL;
    }

    history->max_trades = config->max_trades;
    history->record_all_trades = config->record_all_trades;
    pthread_mutex_init(&history->lock, NULL);

    LOG_INFO("Trade history created with max trades: %d", history->max_trades);
    return history;
}

void trade_history_destroy(TradeHistory* history) {
    if (!history) return;

    pthread_mutex_lock(&history->lock);
    TradeRecord* current = history->trades;
    while (current) {
        TradeRecord* next = current->next;
        free(current);
        current = next;
    }
    pthread_mutex_unlock(&history->lock);

    pthread_mutex_destroy(&history->lock);
    free(history);
    LOG_INFO("Trade history destroyed");
}

int trade_history_add_trade(TradeHistory* history, const TradeMessage* trade) {
    if (!history || !trade) return -1;

    pthread_mutex_lock(&history->lock);

    if (history->trade_count >= history->max_trades) {
        TradeRecord* oldest = history->trades;
        history->trades = oldest->next;
        free(oldest);
        history->trade_count--;
    }

    TradeRecord* record = calloc(1, sizeof(TradeRecord));
    if (!record) {
        LOG_ERROR("Failed to allocate trade record");
        pthread_mutex_unlock(&history->lock);
        return -1;
    }

    memcpy(&record->trade, trade, sizeof(TradeMessage));
    record->next = history->trades;
    history->trades = record;
    history->trade_count++;

    LOG_INFO("Trade recorded: %s %.2f x %d", trade->symbol, 
             trade->price, trade->quantity);

    pthread_mutex_unlock(&history->lock);
    return 0;
}

int trade_history_get_trades(TradeHistory* history, TradeMessage* trades, 
                           int max_trades, int* num_trades) {
    if (!history || !trades || !num_trades) return -1;

    pthread_mutex_lock(&history->lock);
    *num_trades = 0;
    TradeRecord* current = history->trades;

    while (current && *num_trades < max_trades) {
        memcpy(&trades[*num_trades], &current->trade, sizeof(TradeMessage));
        (*num_trades)++;
        current = current->next;
    }

    pthread_mutex_unlock(&history->lock);
    return 0;
}

double trade_history_get_avg_price(TradeHistory* history, const char* symbol) {
    if (!history || !symbol) return 0.0;

    double total_price = 0.0;
    int count = 0;

    pthread_mutex_lock(&history->lock);
    TradeRecord* current = history->trades;

    while (current) {
        if (strcmp(current->trade.symbol, symbol) == 0) {
            total_price += current->trade.price;
            count++;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&history->lock);
    return count > 0 ? total_price / count : 0.0;
}

int trade_history_get_volume(TradeHistory* history, const char* symbol) {
    if (!history || !symbol) return 0;

    int total_volume = 0;

    pthread_mutex_lock(&history->lock);
    TradeRecord* current = history->trades;

    while (current) {
        if (strcmp(current->trade.symbol, symbol) == 0) {
            total_volume += current->trade.quantity;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&history->lock);
    return total_volume;
}

double trade_history_get_vwap(TradeHistory* history, const char* symbol) {
    if (!history || !symbol) return 0.0;

    double volume_price = 0.0;
    int total_volume = 0;

    pthread_mutex_lock(&history->lock);
    TradeRecord* current = history->trades;

    while (current) {
        if (strcmp(current->trade.symbol, symbol) == 0) {
            volume_price += current->trade.price * current->trade.quantity;
            total_volume += current->trade.quantity;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&history->lock);
    return total_volume > 0 ? volume_price / total_volume : 0.0;
}
