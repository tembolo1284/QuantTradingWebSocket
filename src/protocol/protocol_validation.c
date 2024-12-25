#include "protocol/protocol_validation.h"
#include "protocol/protocol_constants.h"
#include "utils/logging.h"
#include <string.h>
#include <ctype.h>

bool validate_symbol(const char* symbol) {
    if (!symbol || strlen(symbol) == 0 || strlen(symbol) >= MAX_SYMBOL_LENGTH) {
        LOG_ERROR("Invalid symbol length: %s", symbol ? symbol : "NULL");
        return false;
    }

    for (const char* p = symbol; *p; p++) {
        if (!isupper(*p)) {
            LOG_ERROR("Invalid symbol character: %c in %s", *p, symbol);
            return false;
        }
    }
    return true;
}

bool validate_order_id(const char* order_id) {
    if (!order_id || strlen(order_id) == 0 || strlen(order_id) >= MAX_ORDER_ID_LENGTH) {
        LOG_ERROR("Invalid order ID length: %s", order_id ? order_id : "NULL");
        return false;
    }

    for (const char* p = order_id; *p; p++) {
        if (!isalnum(*p) && *p != '_' && *p != '-') {
            LOG_ERROR("Invalid order ID character: %c in %s", *p, order_id);
            return false;
        }
    }
    return true;
}

bool validate_trader_id(const char* trader_id) {
    if (!trader_id || strlen(trader_id) == 0 || strlen(trader_id) >= MAX_TRADER_ID_LENGTH) {
        LOG_ERROR("Invalid trader ID length: %s", trader_id ? trader_id : "NULL");
        return false;
    }

    for (const char* p = trader_id; *p; p++) {
        if (!isalnum(*p) && *p != '_') {
            LOG_ERROR("Invalid trader ID character: %c in %s", *p, trader_id);
            return false;
        }
    }
    return true;
}

bool validate_price(double price) {
    if (price < MIN_PRICE || price > MAX_PRICE) {
        LOG_ERROR("Invalid price: %.2f", price);
        return false;
    }
    return true;
}

bool validate_quantity(int quantity) {
    if (quantity < MIN_QUANTITY || quantity > MAX_QUANTITY) {
        LOG_ERROR("Invalid quantity: %d", quantity);
        return false;
    }
    return true;
}

bool validate_order_message(const OrderMessage* order, char* error_msg, size_t error_size) {
    if (!order) {
        strncpy(error_msg, "Null order message", error_size);
        LOG_ERROR("Null order message");
        return false;
    }

    if (!validate_order_id(order->order_id)) {
        snprintf(error_msg, error_size, "Invalid order ID: %s", order->order_id);
        return false;
    }

    if (!validate_trader_id(order->trader_id)) {
        snprintf(error_msg, error_size, "Invalid trader ID: %s", order->trader_id);
        return false;
    }

    if (!validate_symbol(order->stock_symbol)) {
        snprintf(error_msg, error_size, "Invalid symbol: %s", order->stock_symbol);
        return false;
    }

    if (!validate_price(order->price)) {
        snprintf(error_msg, error_size, "Invalid price: %.2f", order->price);
        return false;
    }

    if (!validate_quantity(order->quantity)) {
        snprintf(error_msg, error_size, "Invalid quantity: %d", order->quantity);
        return false;
    }

    LOG_DEBUG("Order message validated successfully: %s", order->order_id);
    return true;
}

bool validate_trade_message(const TradeMessage* trade, char* error_msg, size_t error_size) {
    if (!trade) {
        strncpy(error_msg, "Null trade message", error_size);
        LOG_ERROR("Null trade message");
        return false;
    }

    if (!validate_symbol(trade->symbol)) {
        snprintf(error_msg, error_size, "Invalid symbol: %s", trade->symbol);
        return false;
    }

    if (!validate_order_id(trade->buy_order_id)) {
        snprintf(error_msg, error_size, "Invalid buy order ID: %s", trade->buy_order_id);
        return false;
    }

    if (!validate_order_id(trade->sell_order_id)) {
        snprintf(error_msg, error_size, "Invalid sell order ID: %s", trade->sell_order_id);
        return false;
    }

    if (!validate_price(trade->price)) {
        snprintf(error_msg, error_size, "Invalid price: %.2f", trade->price);
        return false;
    }

    if (!validate_quantity(trade->quantity)) {
        snprintf(error_msg, error_size, "Invalid quantity: %d", trade->quantity);
        return false;
    }

    LOG_DEBUG("Trade message validated successfully");
    return true;
}

bool validate_book_snapshot(const BookSnapshot* snapshot, char* error_msg, size_t error_size) {
    if (!snapshot) {
        strncpy(error_msg, "Null book snapshot", error_size);
        LOG_ERROR("Null book snapshot");
        return false;
    }

    if (!validate_symbol(snapshot->symbol)) {
        snprintf(error_msg, error_size, "Invalid symbol: %s", snapshot->symbol);
        return false;
    }

    if (snapshot->num_bids < 0 || snapshot->num_asks < 0) {
        snprintf(error_msg, error_size, "Invalid number of orders: bids=%d, asks=%d",
                snapshot->num_bids, snapshot->num_asks);
        LOG_ERROR("Invalid order book levels");
        return false;
    }

    for (int i = 0; i < snapshot->num_bids; i++) {
        if (!validate_price(snapshot->bid_prices[i]) || 
            !validate_quantity(snapshot->bid_quantities[i])) {
            snprintf(error_msg, error_size, "Invalid bid: price=%.2f, quantity=%d",
                    snapshot->bid_prices[i], snapshot->bid_quantities[i]);
            return false;
        }
    }

    for (int i = 0; i < snapshot->num_asks; i++) {
        if (!validate_price(snapshot->ask_prices[i]) || 
            !validate_quantity(snapshot->ask_quantities[i])) {
            snprintf(error_msg, error_size, "Invalid ask: price=%.2f, quantity=%d",
                    snapshot->ask_prices[i], snapshot->ask_quantities[i]);
            return false;
        }
    }

    LOG_DEBUG("Book snapshot validated successfully");
    return true;
}

bool validate_server_status(const ServerStatus* status, char* error_msg, size_t error_size) {
    if (!status) {
        strncpy(error_msg, "Null server status", error_size);
        LOG_ERROR("Null server status");
        return false;
    }

    if (status->num_connected_clients < 0) {
        snprintf(error_msg, error_size, "Invalid client count: %d", 
                status->num_connected_clients);
        LOG_ERROR("Invalid client count");
        return false;
    }

    if (status->num_active_orders < 0) {
        snprintf(error_msg, error_size, "Invalid order count: %d",
                status->num_active_orders);
        LOG_ERROR("Invalid order count");
        return false;
    }

    LOG_DEBUG("Server status validated successfully");
    return true;
}
