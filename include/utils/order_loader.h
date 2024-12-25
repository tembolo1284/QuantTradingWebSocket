#ifndef UTILS_ORDER_LOADER_H
#define UTILS_ORDER_LOADER_H

#include "trading_engine/order_book.h"
#include <stdbool.h>

// Function to load orders from a CSV file
// Returns number of orders successfully loaded, or -1 on error
int load_orders_from_file(const char* filename, OrderBook* book);

#endif /* UTILS_ORDER_LOADER_H */
