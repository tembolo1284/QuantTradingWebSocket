#include "trading/order_book.h"
#include "net/websocket_server.h"
#include "utils/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#define DEFAULT_PORT 8080

static volatile sig_atomic_t running = 1;
static OrderBook* book = NULL;
static WebSocketServer* server = NULL;

static void signal_handler(int sig) {
    LOG_INFO("Received shutdown signal %d", sig);
    running = 0;
    
    // Signal server to stop if it exists
    if (server) {
        ws_server_request_shutdown(server);
    }
}

// Callback when a client connects
static void on_client_connect(WebSocketClient* client) {
    (void)client;
    LOG_INFO("New client connected");
    // Send current order book state
    // TODO: Implement order book serialization and send
}

// Callback when a client disconnects
static void on_client_disconnect(WebSocketClient* client) {
    (void)client;
    LOG_INFO("Client disconnected");
}

// Handle incoming messages from clients
static void on_client_message(WebSocketClient* client, const uint8_t* data, size_t len) {
    (void)client;
    (void)data;
    LOG_DEBUG("Received message from client, length: %zu", len);
    // TODO: Parse message and handle different command types:
    // - Add Order
    // - Cancel Order
    // - Query Order Book
    // - Subscribe to Market Data
}

int main(int argc, char* argv[]) {
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Quant Trading Server");
    
    // Setup more robust signal handling
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // Restart interrupted system calls
    
    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_ERROR("Failed to set up signal handlers");
        return 1;
    }

    uint16_t port = DEFAULT_PORT;
    if (argc > 1) {
        port = (uint16_t)atoi(argv[1]);
    }

    // Create order book
    book = order_book_create("AAPL");
    if (!book) {
        LOG_ERROR("Failed to create order book");
        return 1;
    }
    LOG_INFO("Order book created successfully");

    // Create WebSocket server
    WebSocketServerConfig config = {
        .port = port,
        .on_client_connect = on_client_connect,
        .on_client_disconnect = on_client_disconnect,
        .on_client_message = on_client_message
    };
    server = ws_server_create(&config);
    if (!server) {
        LOG_ERROR("Failed to create WebSocket server");
        order_book_destroy(book);
        return 1;
    }
    LOG_INFO("Trading server started on port %u. Press Ctrl+C to exit.", port);

    // Main server loop
    while (running) {
        ws_server_process(server);
        
        // Periodic tasks
        static time_t last_status = 0;
        time_t now = time(NULL);
        if (now - last_status >= 10) {
            LOG_INFO("Server status - Best Bid: %.2f, Best Ask: %.2f",
                    order_book_get_best_bid(book),
                    order_book_get_best_ask(book));
            last_status = now;
        }
        
        // Small sleep to prevent busy-waiting
        usleep(10000);  // 10ms sleep
    }

    // Cleanup
    LOG_INFO("Shutting down server...");
    
    // Destroy server first to close all client connections
    if (server) {
        ws_server_destroy(server);
        server = NULL;
    }
    
    // Destroy order book
    if (book) {
        order_book_destroy(book);
        book = NULL;
    }

    LOG_INFO("Server shutdown complete");
    return 0;
}
