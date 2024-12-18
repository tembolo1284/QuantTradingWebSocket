#include "trading/order_book.h"
#include "net/websocket_server.h"
#include "utils/json_utils.h"
#include "utils/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define DEFAULT_PORT 8080
static volatile bool running = true;
static OrderBook* book = NULL;
static WebSocketServer* server = NULL;

static void handle_signal(int sig) {
    (void)sig;
    LOG_INFO("Received shutdown signal");
    running = false;
    if (server) {
        ws_server_request_shutdown(server);
    }
}

// Callback when a client connects
static void on_client_connect(WebSocketClient* client) {
    (void)client;
    LOG_INFO("New client connected");
}

// Callback when a client disconnects
static void on_client_disconnect(WebSocketClient* client) {
    (void)client;
    LOG_INFO("Client disconnected");
}

// Handle incoming messages from clients
// Handle incoming messages from clients
static void on_client_message(WebSocketClient* client, const uint8_t* data, size_t len) {
    // First, ensure the data is a valid null-terminated string
    char* json_str = malloc(len + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for message");
        return;
    }
    
    // Copy data, ensuring null-termination
    memcpy(json_str, data, len);
    json_str[len] = '\0';

    // Log the raw received data for debugging
    LOG_DEBUG("Received raw message (len=%zu): %s", len, json_str);

    // Parse the JSON message
    ParsedMessage parsed_msg;
    if (!json_parse_message(json_str, &parsed_msg)) {
        LOG_ERROR("Failed to parse JSON message");
        free(json_str);
        return;
    }

    // Handle different message types
    switch (parsed_msg.type) {
        case JSON_MSG_ORDER_ADD: {
            // Create and add order to the book
            Order order = parsed_msg.data.order_add.order;
            LOG_INFO("Adding order: price=%.2f, quantity=%u, is_buy=%d", 
                     order.price, order.quantity, order.is_buy);
            
            if (order_book_add(book, &order)) {
                LOG_INFO("Order added successfully");
            } else {
                LOG_ERROR("Failed to add order");
            }
            break;
        }

        case JSON_MSG_BOOK_QUERY: {
            // Serialize and send order book
            char* book_json = json_serialize_order_book(book);
            if (book_json) {
                LOG_INFO("Sending order book snapshot");
                ws_client_send(client, (const uint8_t*)book_json, strlen(book_json));
                free(book_json);
            } else {
                LOG_ERROR("Failed to serialize order book");
            }
            break;
        }

        default:
            LOG_WARN("Unhandled message type");
            break;
    }

    // Free parsed message resources
    json_free_parsed_message(&parsed_msg);
    free(json_str);
}

int main(int argc, char* argv[]) {
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Quant Trading Server");
    
    uint16_t port = DEFAULT_PORT;
    if (argc > 1) {
        port = (uint16_t)atoi(argv[1]);
    }

    // Setup signal handlers
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_ERROR("Failed to set up signal handlers");
        return 1;
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
        if (now - last_status >= 30) {
            LOG_INFO("Server status - Best Bid: %.2f, Best Ask: %.2f",
                    order_book_get_best_bid(book),
                    order_book_get_best_ask(book));
            last_status = now;
        }
        
        usleep(10000);  // 10ms sleep
    }

    // Cleanup
    LOG_INFO("Shutting down server...");
    
    if (server) {
        ws_server_destroy(server);
        server = NULL;
    }
    
    if (book) {
        order_book_destroy(book);
        book = NULL;
    }

    LOG_INFO("Server shutdown complete");
    return 0;
}
