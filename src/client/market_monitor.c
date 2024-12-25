#include "client/market_monitor.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#define MAX_SUBSCRIPTIONS 100

typedef struct {
    char symbol[16];
    BookSnapshot latest_book;
    TradeMessage latest_trade;
    bool has_update;
} MarketSymbol;

struct MarketMonitor {
    MarketSymbol* symbols;
    int symbol_count;
    int max_symbols;
    int update_interval_ms;
    bool display_full_depth;
    pthread_mutex_t lock;
};

static void print_book_snapshot(const BookSnapshot* snapshot, bool full_depth) {
    printf("\n%s Order Book:\n", snapshot->symbol);
    printf("Bids:\t\t\tAsks:\n");
    printf("Price\tSize\t\tPrice\tSize\n");
    printf("----------------------------------------\n");

    int depth = full_depth ? snapshot->num_bids : (snapshot->num_bids > 5 ? 5 : snapshot->num_bids);
    
    for (int i = 0; i < depth; i++) {
        printf("%.2f\t%d", snapshot->bid_prices[i], snapshot->bid_quantities[i]);
        if (i < snapshot->num_asks) {
            printf("\t\t%.2f\t%d", snapshot->ask_prices[i], snapshot->ask_quantities[i]);
        }
        printf("\n");
    }
    printf("\n");
}

MarketMonitor* market_monitor_create(const MarketMonitorConfig* config) {
    MarketMonitor* monitor = calloc(1, sizeof(MarketMonitor));
    if (!monitor) {
        LOG_ERROR("Failed to allocate market monitor");
        return NULL;
    }

    monitor->symbols = calloc(config->max_symbols, sizeof(MarketSymbol));
    if (!monitor->symbols) {
        LOG_ERROR("Failed to allocate symbol array");
        free(monitor);
        return NULL;
    }

    monitor->max_symbols = config->max_symbols;
    monitor->update_interval_ms = config->update_interval_ms;
    monitor->display_full_depth = config->display_full_depth;
    pthread_mutex_init(&monitor->lock, NULL);

    LOG_INFO("Market monitor created with max symbols: %d", monitor->max_symbols);
    return monitor;
}

void market_monitor_destroy(MarketMonitor* monitor) {
    if (!monitor) return;

    pthread_mutex_lock(&monitor->lock);
    for (int i = 0; i < monitor->symbol_count; i++) {
        free(monitor->symbols[i].latest_book.bid_prices);
        free(monitor->symbols[i].latest_book.bid_quantities);
        free(monitor->symbols[i].latest_book.ask_prices);
        free(monitor->symbols[i].latest_book.ask_quantities);
    }
    free(monitor->symbols);
    pthread_mutex_unlock(&monitor->lock);

    pthread_mutex_destroy(&monitor->lock);
    free(monitor);
    LOG_INFO("Market monitor destroyed");
}

int market_monitor_subscribe(MarketMonitor* monitor, const char* symbol) {
    if (!monitor || !symbol) return -1;

    pthread_mutex_lock(&monitor->lock);

    if (monitor->symbol_count >= monitor->max_symbols) {
        LOG_ERROR("Maximum symbols reached");
        pthread_mutex_unlock(&monitor->lock);
        return -1;
    }

    for (int i = 0; i < monitor->symbol_count; i++) {
        if (strcmp(monitor->symbols[i].symbol, symbol) == 0) {
            pthread_mutex_unlock(&monitor->lock);
            return 0; // Already subscribed
        }
    }

    strncpy(monitor->symbols[monitor->symbol_count].symbol, symbol, sizeof(monitor->symbols[0].symbol) - 1);
    monitor->symbol_count++;

    LOG_INFO("Subscribed to symbol: %s", symbol);
    pthread_mutex_unlock(&monitor->lock);
    return 0;
}

int market_monitor_update_book(MarketMonitor* monitor, const BookSnapshot* snapshot) {
    if (!monitor || !snapshot) return -1;

    pthread_mutex_lock(&monitor->lock);
    for (int i = 0; i < monitor->symbol_count; i++) {
        if (strcmp(monitor->symbols[i].symbol, snapshot->symbol) == 0) {
            memcpy(&monitor->symbols[i].latest_book, snapshot, sizeof(BookSnapshot));
            monitor->symbols[i].has_update = true;
            pthread_mutex_unlock(&monitor->lock);
            LOG_DEBUG("Updated book for symbol: %s", snapshot->symbol);
            return 0;
        }
    }
    pthread_mutex_unlock(&monitor->lock);
    return -1;
}

int market_monitor_update_trade(MarketMonitor* monitor, const TradeMessage* trade) {
    if (!monitor || !trade) return -1;

    pthread_mutex_lock(&monitor->lock);
    for (int i = 0; i < monitor->symbol_count; i++) {
        if (strcmp(monitor->symbols[i].symbol, trade->symbol) == 0) {
            memcpy(&monitor->symbols[i].latest_trade, trade, sizeof(TradeMessage));
            printf("\nTRADE: %s %.2f x %d\n", 
                   trade->symbol, trade->price, trade->quantity);
            pthread_mutex_unlock(&monitor->lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&monitor->lock);
    return -1;
}

void market_monitor_display(MarketMonitor* monitor) {
    if (!monitor) return;

    pthread_mutex_lock(&monitor->lock);
    printf("\033[2J\033[H"); // Clear screen and move cursor to top

    for (int i = 0; i < monitor->symbol_count; i++) {
        if (monitor->symbols[i].has_update) {
            print_book_snapshot(&monitor->symbols[i].latest_book, 
                              monitor->display_full_depth);
        }
    }
    pthread_mutex_unlock(&monitor->lock);
}

void market_monitor_clear(MarketMonitor* monitor) {
    if (!monitor) return;
    
    pthread_mutex_lock(&monitor->lock);
    for (int i = 0; i < monitor->symbol_count; i++) {
        monitor->symbols[i].has_update = false;
    }
    pthread_mutex_unlock(&monitor->lock);
    printf("\033[2J\033[H"); // Clear screen
}
