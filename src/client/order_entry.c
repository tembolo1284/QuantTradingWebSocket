#include "client/order_entry.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct OrderRecord {
    char order_id[32];
    char symbol[16];
    double price;
    int quantity;
    bool is_buy;
    struct OrderRecord* next;
} OrderRecord;

struct OrderEntry {
    OrderRecord* active_orders;
    int active_count;
    double total_notional;
    int max_orders;
    double max_notional;
    char* trader_id;
    pthread_mutex_t lock;
};

OrderEntry* order_entry_create(const OrderEntryConfig* config) {
    OrderEntry* entry = calloc(1, sizeof(OrderEntry));
    if (!entry) {
        LOG_ERROR("Failed to allocate order entry");
        return NULL;
    }

    entry->max_orders = config->max_orders;
    entry->max_notional = config->max_notional;
    entry->trader_id = strdup(config->trader_id);
    pthread_mutex_init(&entry->lock, NULL);

    LOG_INFO("Order entry created for trader %s", entry->trader_id);
    return entry;
}

void order_entry_destroy(OrderEntry* entry) {
    if (!entry) return;

    pthread_mutex_lock(&entry->lock);
    OrderRecord* current = entry->active_orders;
    while (current) {
        OrderRecord* next = current->next;
        free(current);
        current = next;
    }
    pthread_mutex_unlock(&entry->lock);

    pthread_mutex_destroy(&entry->lock);
    free(entry->trader_id);
    free(entry);
    LOG_INFO("Order entry destroyed");
}

int order_entry_submit(OrderEntry* entry, const char* symbol, double price, 
                      int quantity, bool is_buy) {
    if (!entry || !symbol) {
        LOG_ERROR("Invalid parameters for order submission");
        return -1;
    }

    double new_notional = price * quantity;
    pthread_mutex_lock(&entry->lock);

    if (entry->active_count >= entry->max_orders) {
        LOG_ERROR("Maximum orders reached (%d)", entry->max_orders);
        pthread_mutex_unlock(&entry->lock);
        return -1;
    }

    if (entry->total_notional + new_notional > entry->max_notional) {
        LOG_ERROR("Maximum notional value exceeded (%.2f)", entry->max_notional);
        pthread_mutex_unlock(&entry->lock);
        return -1;
    }

    OrderRecord* record = calloc(1, sizeof(OrderRecord));
    if (!record) {
        LOG_ERROR("Failed to allocate order record");
        pthread_mutex_unlock(&entry->lock);
        return -1;
    }

    snprintf(record->order_id, sizeof(record->order_id), "ORD%d", entry->active_count + 1);
    strncpy(record->symbol, symbol, sizeof(record->symbol) - 1);
    record->price = price;
    record->quantity = quantity;
    record->is_buy = is_buy;

    record->next = entry->active_orders;
    entry->active_orders = record;
    entry->active_count++;
    entry->total_notional += new_notional;

    LOG_INFO("Order submitted: %s %s %.2f x %d %s", 
             record->order_id, symbol, price, quantity, is_buy ? "BUY" : "SELL");

    pthread_mutex_unlock(&entry->lock);
    return 0;
}

int order_entry_cancel(OrderEntry* entry, const char* order_id) {
    if (!entry || !order_id) {
        LOG_ERROR("Invalid parameters for order cancellation");
        return -1;
    }

    pthread_mutex_lock(&entry->lock);

    OrderRecord* prev = NULL;
    OrderRecord* current = entry->active_orders;

    while (current) {
        if (strcmp(current->order_id, order_id) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                entry->active_orders = current->next;
            }

            entry->total_notional -= (current->price * current->quantity);
            entry->active_count--;

            LOG_INFO("Order cancelled: %s", order_id);
            free(current);
            pthread_mutex_unlock(&entry->lock);
            return 0;
        }
        prev = current;
        current = current->next;
    }

    LOG_ERROR("Order not found: %s", order_id);
    pthread_mutex_unlock(&entry->lock);
    return -1;
}

int64_t order_entry_get_active_orders(OrderEntry* entry) {
    return entry ? entry->active_count : 0;
}

double order_entry_get_total_notional(OrderEntry* entry) {
    return entry ? entry->total_notional : 0.0;
}
