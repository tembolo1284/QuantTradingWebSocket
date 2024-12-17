#include "trading/order_book.h"
#include "net/websocket.h"
#include "utils/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static volatile bool running = true;

static void handle_signal(int sig) {
    (void)sig;
    LOG_INFO("Received shutdown signal");
    running = false;
}

static void on_message(const uint8_t* data, size_t len, void* user_data) {
    OrderBook* book = (OrderBook*)user_data;
    
    LOG_DEBUG("Received WebSocket message of length %zu bytes", len);
    if (len > 0) {
        // For debugging, print the first few bytes of the message
        char preview[32] = {0};
        size_t preview_len = len < sizeof(preview) - 1 ? len : sizeof(preview) - 1;
        memcpy(preview, data, preview_len);
        LOG_DEBUG("Message preview: %s", preview);
    }
    
    // Example: create a sample order
    Order* order = order_create("AAPL", 150.0, 100, true);
    if (order) {
        if (order_book_add(book, order)) {
            LOG_INFO("Added order: ID=%lu, Price=%.2f, Quantity=%u",
                    order->id, order->price, order->quantity);
                   
            LOG_INFO("Best Bid: %.2f, Best Ask: %.2f",
                    order_book_get_best_bid(book),
                    order_book_get_best_ask(book));
        } else {
            LOG_ERROR("Failed to add order to book");
        }
        free(order);
    } else {
        LOG_ERROR("Failed to create order");
    }
}

static void on_error(ErrorCode error, void* user_data) {
    (void)user_data;
    LOG_ERROR("WebSocket error: %d", error);
    
    switch (error) {
        case ERROR_NETWORK:
            LOG_ERROR("Network error occurred");
            break;
        case ERROR_TIMEOUT:
            LOG_ERROR("Connection timeout");
            break;
        case ERROR_INVALID_PARAM:
            LOG_ERROR("Invalid parameter");
            break;
        case ERROR_MEMORY:
            LOG_ERROR("Memory allocation error");
            break;
        default:
            LOG_ERROR("Unknown error");
            break;
    }
}

int main(int argc, char* argv[]) {
    // Initialize logging
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Quant Trading System");

    if (argc != 3) {
        LOG_ERROR("Invalid arguments. Usage: %s <host> <port>", argv[0]);
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    LOG_INFO("Connecting to %s:%s", argv[1], argv[2]);

    // Setup signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create order book
    OrderBook* book = order_book_create("AAPL");
    if (!book) {
        LOG_ERROR("Failed to create order book");
        return 1;
    }
    LOG_INFO("Order book created successfully");

    // Setup WebSocket callbacks
    WebSocketCallbacks callbacks = {
        .on_message = on_message,
        .on_error = on_error,
        .user_data = book
    };

    // Create WebSocket connection
    LOG_DEBUG("Creating WebSocket connection...");
    WebSocket* ws = ws_create(argv[1], (uint16_t)atoi(argv[2]), &callbacks);
    if (!ws) {
        LOG_ERROR("Failed to create WebSocket connection");
        order_book_destroy(book);
        return 1;
    }

    LOG_INFO("Trading system started. Press Ctrl+C to exit.");

    // Main event loop
    while (running) {
        ws_process(ws);
        usleep(100);  // Small sleep to prevent busy-waiting
        
        // Optional: Add periodic status logging
        static time_t last_status = 0;
        time_t now = time(NULL);
        if (now - last_status >= 5) {  // Log status every 5 seconds
            LOG_DEBUG("System running - Best Bid: %.2f, Best Ask: %.2f",
                     order_book_get_best_bid(book),
                     order_book_get_best_ask(book));
            last_status = now;
        }
    }

    // Cleanup
    LOG_INFO("Shutting down...");
    ws_close(ws);
    order_book_destroy(book);
    LOG_INFO("Trading system shutdown complete");
    
    return 0;
}
