#include "server/market_data.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct MarketData {
    pthread_mutex_t lock;
    pthread_t snapshot_thread;
    bool running;
    
    char symbols[MAX_SYMBOLS][16];
    OrderBook* books[MAX_SYMBOLS];
    int book_count;
    
    int snapshot_interval_ms;
    int max_depth;
};

static void* snapshot_thread(void* arg) {
    MarketData* market = (MarketData*)arg;
    
    while (market->running) {
        struct timespec ts = {
            .tv_sec = market->snapshot_interval_ms / 1000,
            .tv_nsec = (market->snapshot_interval_ms % 1000) * 1000000
        };
        nanosleep(&ts, NULL);
        
        pthread_mutex_lock(&market->lock);
        
        for (int i = 0; i < market->book_count; i++) {
            BookSnapshot snapshot = {0};
            strncpy(snapshot.symbol, market->symbols[i], sizeof(snapshot.symbol) - 1);
            
            snapshot.bid_prices = malloc(market->max_depth * sizeof(double));
            snapshot.bid_quantities = malloc(market->max_depth * sizeof(int));
            snapshot.ask_prices = malloc(market->max_depth * sizeof(double));
            snapshot.ask_quantities = malloc(market->max_depth * sizeof(int));
            
            if (!snapshot.bid_prices || !snapshot.bid_quantities || 
                !snapshot.ask_prices || !snapshot.ask_quantities) {
                LOG_ERROR("Failed to allocate memory for book snapshot");
                free(snapshot.bid_prices);
                free(snapshot.bid_quantities);
                free(snapshot.ask_prices);
                free(snapshot.ask_quantities);
                continue;
            }
            
            // TODO: Fill snapshot data from order book
            // This would involve traversing the AVL trees to get top N levels
            
            free(snapshot.bid_prices);
            free(snapshot.bid_quantities);
            free(snapshot.ask_prices);
            free(snapshot.ask_quantities);
        }
        
        pthread_mutex_unlock(&market->lock);
    }
    
    return NULL;
}

MarketData* market_data_create(const MarketDataConfig* config) {
    MarketData* market = calloc(1, sizeof(MarketData));
    if (!market) return NULL;
    
    market->snapshot_interval_ms = config->snapshot_interval_ms;
    market->max_depth = config->max_depth;
    pthread_mutex_init(&market->lock, NULL);
    
    return market;
}

void market_data_destroy(MarketData* market) {
    if (!market) return;
    
    if (market->running) {
        market_data_stop_snapshot_timer(market);
    }
    
    for (int i = 0; i < market->book_count; i++) {
        order_book_destroy(market->books[i]);
    }
    
    pthread_mutex_destroy(&market->lock);
    free(market);
}

int market_data_start_snapshot_timer(MarketData* market) {
    if (!market || market->running) return -1;
    
    market->running = true;
    if (pthread_create(&market->snapshot_thread, NULL, snapshot_thread, market) != 0) {
        market->running = false;
        return -1;
    }
    
    return 0;
}

int market_data_stop_snapshot_timer(MarketData* market) {
    if (!market || !market->running) return -1;
    
    market->running = false;
    pthread_join(market->snapshot_thread, NULL);
    return 0;
}

int market_data_update_book(MarketData* market, const char* symbol, const OrderBook* book) {
    if (!market || !symbol || !book) return -1;
    
    pthread_mutex_lock(&market->lock);
    
    int index = -1;
    for (int i = 0; i < market->book_count; i++) {
        if (strcmp(market->symbols[i], symbol) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1 && market->book_count < MAX_SYMBOLS) {
        index = market->book_count++;
        strncpy(market->symbols[index], symbol, sizeof(market->symbols[0]) - 1);
        market->books[index] = order_book_create();
    }
    
    if (index != -1 && market->books[index]) {
        // TODO: Implement deep copy of order book data
    }
    
    pthread_mutex_unlock(&market->lock);
    return index != -1 ? 0 : -1;
}

int market_data_get_snapshot(MarketData* market, const char* symbol, BookSnapshot* snapshot) {
    if (!market || !symbol || !snapshot) return -1;
    
    pthread_mutex_lock(&market->lock);
    
    int result = -1;
    for (int i = 0; i < market->book_count; i++) {
        if (strcmp(market->symbols[i], symbol) == 0) {
            strncpy(snapshot->symbol, symbol, sizeof(snapshot->symbol) - 1);
            
            snapshot->bid_prices = malloc(market->max_depth * sizeof(double));
            snapshot->bid_quantities = malloc(market->max_depth * sizeof(int));
            snapshot->ask_prices = malloc(market->max_depth * sizeof(double));
            snapshot->ask_quantities = malloc(market->max_depth * sizeof(int));
            
            if (!snapshot->bid_prices || !snapshot->bid_quantities || 
                !snapshot->ask_prices || !snapshot->ask_quantities) {
                free(snapshot->bid_prices);
                free(snapshot->bid_quantities);
                free(snapshot->ask_prices);
                free(snapshot->ask_quantities);
                break;
            }
            
            // TODO: Fill snapshot data from market->books[i]
            result = 0;
            break;
        }
    }
    
    pthread_mutex_unlock(&market->lock);
    return result;
}

int market_data_get_all_snapshots(MarketData* market, BookSnapshot** snapshots, int* num_snapshots) {
    if (!market || !snapshots || !num_snapshots) return -1;
    
    pthread_mutex_lock(&market->lock);
    
    *snapshots = calloc(market->book_count, sizeof(BookSnapshot));
    if (!*snapshots) {
        pthread_mutex_unlock(&market->lock);
        return -1;
    }
    
    *num_snapshots = 0;
    for (int i = 0; i < market->book_count; i++) {
        BookSnapshot* snapshot = &(*snapshots)[i];
        strncpy(snapshot->symbol, market->symbols[i], sizeof(snapshot->symbol) - 1);
        
        snapshot->bid_prices = malloc(market->max_depth * sizeof(double));
        snapshot->bid_quantities = malloc(market->max_depth * sizeof(int));
        snapshot->ask_prices = malloc(market->max_depth * sizeof(double));
        snapshot->ask_quantities = malloc(market->max_depth * sizeof(int));
        
        if (!snapshot->bid_prices || !snapshot->bid_quantities || 
            !snapshot->ask_prices || !snapshot->ask_quantities) {
            for (int j = 0; j <= i; j++) {
                free((*snapshots)[j].bid_prices);
                free((*snapshots)[j].bid_quantities);
                free((*snapshots)[j].ask_prices);
                free((*snapshots)[j].ask_quantities);
            }
            free(*snapshots);
            pthread_mutex_unlock(&market->lock);
            return -1;
        }
        
        // TODO: Fill snapshot data from market->books[i]
        (*num_snapshots)++;
    }
    
    pthread_mutex_unlock(&market->lock);
    return 0;
}

int market_data_get_symbol_count(const MarketData* market) {
    if (!market) return 0;
    return market->book_count;
}

int market_data_get_total_orders(const MarketData* market) {
    if (!market) return 0;
    
    int total = 0;
    pthread_mutex_lock(&market->lock);
    
    for (int i = 0; i < market->book_count; i++) {
        // TODO: Get order count from each book
    }
    
    pthread_mutex_unlock(&market->lock);
    return total;
}

double market_data_get_total_volume(const MarketData* market) {
    if (!market) return 0.0;
    
    double total = 0.0;
    pthread_mutex_lock(&market->lock);
    
    for (int i = 0; i < market->book_count; i++) {
        // TODO: Calculate total volume from each book
    }
    
    pthread_mutex_unlock(&market->lock);
    return total;
}
