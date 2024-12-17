#include "trading/order_book.h"
#include "net/websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static volatile bool running = true;

static void handle_signal(int sig) {
    running = false;
}

static void on_message(const uint8_t* data, size_t len, void* user_data) {
    OrderBook* book = (OrderBook*)user_data;
    
    // Simple example: parse message as JSON (you'd want a proper JSON parser)
    // Format: {"type": "order", "symbol": "AAPL", "price": 150.0, "quantity": 100, "is_buy": true}
    
    // For demonstration, we'll just create a sample order
    Order* order = order_create("AAPL", 150.0, 100, true);
    if (order) {
        if (order_book_add(book, order)) {
            printf("Added order: ID=%lu, Price=%.2f, Quantity=%u\n",
                   order->id, order->price, order->quantity);
                   
            // Print best bid/ask
            printf("Best Bid: %.2f, Best Ask: %.2f\n",
                   order_book_get_best_bid(book),
                   order_book_get_best_ask(book));
        }
        free(order);
    }
}

static void on_error(ErrorCode error, void* user_data) {
    printf("WebSocket error: %d\n", error);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    // Setup signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create order book
    OrderBook* book = order_book_create("AAPL");
    if (!book) {
        fprintf(stderr, "Failed to create order book\n");
        return 1;
    }

    // Setup WebSocket callbacks
    WebSocketCallbacks callbacks = {
        .on_message = on_message,
        .on_error = on_error,
        .user_data = book
    };

    // Create WebSocket connection
    WebSocket* ws = ws_create(argv[1], (uint16_t)atoi(argv[2]), &callbacks);
    if (!ws) {
        fprintf(stderr, "Failed to create WebSocket connection\n");
        order_book_destroy(book);
        return 1;
    }

    printf("Trading system started. Press Ctrl+C to exit.\n");

    // Main event loop
    while (running) {
        ws_process(ws);
        usleep(100);  // Small sleep to prevent busy-waiting
    }

    // Cleanup
    ws_close(ws);
    order_book_destroy(book);

    printf("\nTrading system shutdown complete.\n");
    return 0;
}
