#include "trading/order_book.h"
#include "net/websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static volatile bool running = true;
static const double SPREAD = 0.10;  // 10 cents spread
static const uint32_t ORDER_QUANTITY = 100;

typedef struct {
    OrderBook* book;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
} MarketMaker;

static void handle_signal(int sig) {
    running = false;
}

static void update_quotes(MarketMaker* mm, double mid_price) {
    // Cancel existing orders
    if (mm->buy_order_id) {
        order_book_cancel(mm->book, mm->buy_order_id);
    }
    if (mm->sell_order_id) {
        order_book_cancel(mm->book, mm->sell_order_id);
    }

    // Create new orders at spread around mid price
    Order* buy_order = order_create("AAPL", mid_price - SPREAD/2, ORDER_QUANTITY, true);
    Order* sell_order = order_create("AAPL", mid_price + SPREAD/2, ORDER_QUANTITY, false);

    if (buy_order && sell_order) {
        if (order_book_add(mm->book, buy_order)) {
            mm->buy_order_id = buy_order->id;
            printf("Added buy order at %.2f\n", buy_order->price);
        }
        if (order_book_add(mm->book, sell_order)) {
            mm->sell_order_id = sell_order->id;
            printf("Added sell order at %.2f\n", sell_order->price);
        }
    }

    free(buy_order);
    free(sell_order);
}

static void on_message(const uint8_t* data, size_t len, void* user_data) {
    MarketMaker* mm = (MarketMaker*)user_data;
    
    // In a real system, parse market data message to get mid price
    // For this example, we'll just use a simple price update
    double mid_price = 150.0;  // Example mid price
    
    update_quotes(mm, mid_price);
}

static void on_error(ErrorCode error, void* user_data) {
    printf("WebSocket error: %d\n", error);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    MarketMaker mm = {0};
    mm.book = order_book_create("AAPL");
    if (!mm.book) {
        fprintf(stderr, "Failed to create order book\n");
        return 1;
    }

    WebSocketCallbacks callbacks = {
        .on_message = on_message,
        .on_error = on_error,
        .user_data = &mm
    };

    WebSocket* ws = ws_create(argv[1], (uint16_t)atoi(argv[2]), &callbacks);
    if (!ws) {
        fprintf(stderr, "Failed to create WebSocket connection\n");
        order_book_destroy(mm.book);
        return 1;
    }

    printf("Market maker started. Press Ctrl+C to exit.\n");

    while (running) {
        ws_process(ws);
        usleep(100000);  // 100ms sleep
    }

    ws_close(ws);
    order_book_destroy(mm.book);
    printf("\nMarket maker shutdown complete.\n");
    return 0;
}
