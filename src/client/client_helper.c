#include "client/client_helper.h"
#include "trading/engine/order_book.h"
#include "trading/protocol/messages.h"
#include "utils/json_utils.h"
#include "utils/logging.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include <stdint.h>

static volatile bool running = true;
static atomic_uint_fast32_t order_counter = ATOMIC_VAR_INIT(0);

uint64_t generate_order_id(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t counter = atomic_fetch_add(&order_counter, 1);
    uint32_t nano_counter = (uint32_t)(ts.tv_nsec / 1000) ^ counter;
    uint64_t id = ((uint64_t)ts.tv_sec << 32) | nano_counter;

    LOG_DEBUG("Generated order ID: %lu (timestamp: %lu, counter: %u)",
              id, (uint64_t)ts.tv_sec, counter);
    return id;
}

void print_usage(void) {
    printf("\nAvailable commands:\n");
    printf("  order buy <price> <quantity> <symbol>   - Place buy order\n");
    printf("  order sell <price> <quantity> <symbol>  - Place sell order\n");
    printf("  cancel <order_id>              - Cancel order\n");
    printf("  book                           - Show order book\n");
    printf("  help                           - Show this help\n");
    printf("  quit                           - Exit client\n\n");
}

void on_error(ErrorCode error, void* user_data) {
    (void)user_data;
    LOG_ERROR("WebSocket error: %s", ws_error_string(error));
    running = false;
}

void print_order_book(const BookSymbol* symbol) {
    time_t now;
    time(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    printf("\nOrder Book for %s\n", symbol->symbol);
    printf("================================================================================\n\n");

    printf("Buy Orders:\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("    Order ID    Symbol       Price    Quantity\n");
    printf("--------------------------------------------------------------------------------\n");

    size_t total_buy_quantity = 0;
    for (size_t i = 0; i < symbol->buy_orders_count; i++) {
        const BookOrder* order = &symbol->buy_orders[i];
        printf("%11lu    %-8s  %9.2f  %9u\n",
               order->id,
               symbol->symbol,
               order->price,
               order->quantity);
        total_buy_quantity += order->quantity;
    }
    if (symbol->buy_orders_count == 0) {
        printf("    No buy orders\n");
    }
    printf("\n");

    printf("Sell Orders:\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("    Order ID    Symbol       Price    Quantity\n");
    printf("--------------------------------------------------------------------------------\n");

    size_t total_sell_quantity = 0;
    for (size_t i = 0; i < symbol->sell_orders_count; i++) {
        const BookOrder* order = &symbol->sell_orders[i];
        printf("%11lu    %-8s  %9.2f  %9u\n",
               order->id,
               symbol->symbol,
               order->price,
               order->quantity);
        total_sell_quantity += order->quantity;
    }
    if (symbol->sell_orders_count == 0) {
        printf("    No sell orders\n");
    }
    printf("\n");

    printf("Summary:\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("Total Buy Orders:  %zu (Volume: %zu)\n", symbol->buy_orders_count, total_buy_quantity);
    printf("Total Sell Orders: %zu (Volume: %zu)\n", symbol->sell_orders_count, total_sell_quantity);
    printf("Timestamp: %s\n\n", timestamp);
}

bool send_book_query(WebSocket* ws, const char* symbol) {
    // Existing implementation, but ensure it returns a boolean
    ParsedMessage msg = {0};
    msg.type = JSON_MSG_BOOK_QUERY;

    if (symbol) {
        strncpy(msg.data.book_query.symbol, symbol, sizeof(msg.data.book_query.symbol) - 1);
        msg.data.book_query.type = BOOK_QUERY_SYMBOL;
    } else {
        msg.data.book_query.type = BOOK_QUERY_ALL;
        msg.data.book_query.symbol[0] = '\0';
    }

    char* json_str = json_serialize_message(&msg);
    if (!json_str) return false;

    bool success = ws_send(ws, (const uint8_t*)json_str, strlen(json_str));
    free(json_str);
    return success;
}

bool send_order(WebSocket* ws, bool is_buy, double price, uint32_t quantity, const char* symbol) {
    LOG_DEBUG("Creating order: %s %.2f %u %s",
             is_buy ? "buy" : "sell", price, quantity, symbol);

    ParsedMessage msg = {0};
    msg.type = JSON_MSG_ORDER_ADD;
    msg.data.order_add.order.id = generate_order_id();
    msg.data.order_add.order.price = price;
    msg.data.order_add.order.quantity = quantity;
    msg.data.order_add.order.is_buy = is_buy;

    strncpy(msg.data.order_add.symbol, symbol, sizeof(msg.data.order_add.symbol) - 1);
    strncpy(msg.data.order_add.order.symbol, symbol, sizeof(msg.data.order_add.order.symbol) - 1);

    char* json_str = json_serialize_message(&msg);
    if (!json_str) {
        LOG_ERROR("Failed to serialize order");
        return false;
    }

    LOG_DEBUG("Sending order JSON: %s", json_str);
    bool success = ws_send(ws, (const uint8_t*)json_str, strlen(json_str));
    free(json_str);

    return success;
}

void send_order_cancel(WebSocket* ws, uint64_t order_id) {
    ParsedMessage msg = {0};
    msg.type = JSON_MSG_ORDER_CANCEL;
    msg.data.order_cancel.order_id = order_id;

    char* json_str = json_serialize_message(&msg);
    if (json_str) {
        LOG_INFO("Sending cancel request for order %lu", order_id);
        ws_send(ws, (const uint8_t*)json_str, strlen(json_str));
        free(json_str);
    }
}
