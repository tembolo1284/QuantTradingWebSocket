#ifndef CLIENT_ORDER_ENTRY_H
#define CLIENT_ORDER_ENTRY_H

#include "protocol/message_types.h"
#include <stdint.h>

typedef struct OrderEntry OrderEntry;

typedef struct {
    char* trader_id;
    int max_orders;
    double max_notional;
} OrderEntryConfig;

OrderEntry* order_entry_create(const OrderEntryConfig* config);
void order_entry_destroy(OrderEntry* entry);

int order_entry_submit(OrderEntry* entry, const char* symbol, double price, 
                      int quantity, bool is_buy);
int order_entry_cancel(OrderEntry* entry, const char* order_id);

int64_t order_entry_get_active_orders(OrderEntry* entry);
double order_entry_get_total_notional(OrderEntry* entry);

#endif /* CLIENT_ORDER_ENTRY_H */
