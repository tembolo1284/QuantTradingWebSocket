#ifndef TRADER_H
#define TRADER_H

#define MAX_TRADER_ID_LENGTH 64
#define MAX_TRADER_NAME_LENGTH 128

// Forward declarations
struct Order;
struct OrderBook;

typedef struct Trader {
    char trader_id[MAX_TRADER_ID_LENGTH];
    char name[MAX_TRADER_NAME_LENGTH];
    double balance;
} Trader;

// Constructor and destructor
Trader* trader_create(const char* trader_id, const char* name, double balance);
void trader_destroy(Trader* trader);

// Getters
const char* trader_get_id(const Trader* trader);
const char* trader_get_name(const Trader* trader);
double trader_get_balance(const Trader* trader);

// Setters
void trader_set_id(Trader* trader, const char* trader_id);
void trader_set_name(Trader* trader, const char* name);
void trader_set_balance(Trader* trader, double balance);

// Trading operations
int trader_place_order(Trader* trader, struct OrderBook* order_book, const struct Order* order);
void trader_update_balance(Trader* trader, double amount);

#endif /* TRADER_H */
