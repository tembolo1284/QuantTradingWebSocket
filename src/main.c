#include "trading/order_handler.h"
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
static WebSocketServer* server = NULL;

static void handle_signal(int sig) {
    (void)sig;
    LOG_INFO("Received shutdown signal");
    running = false;
    if (server) {
        ws_server_request_shutdown(server);
    }
}

static void on_client_connect(WebSocketClient* client) {
    (void)client;
    LOG_INFO("New client connected");
}

static void on_client_disconnect(WebSocketClient* client) {
    (void)client;
    LOG_INFO("Client disconnected");
}

static void on_client_message(WebSocketClient* client, const uint8_t* data, size_t len) {
    // Convert to null-terminated string
    char* json_str = malloc(len + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for message");
        return;
    }
    memcpy(json_str, data, len);
    json_str[len] = '\0';

    LOG_DEBUG("Received message: %s", json_str);

    // Parse the JSON message
    ParsedMessage parsed_msg;
    if (json_parse_message(json_str, &parsed_msg)) {
        switch (parsed_msg.type) {
            case JSON_MSG_ORDER_ADD: {

                if (!order_handler_create_book(parsed_msg.data.order_add.symbol)) {
                    LOG_ERROR("Failed to create/switch order book to symbol %s",
                                parsed_msg.data.order_add.symbol);
                    break;
                }

                // Add order to order book
                OrderHandlingResult result = order_handler_add_order(&parsed_msg.data.order_add.order);
                if (result != ORDER_SUCCESS) {
                    LOG_ERROR("Failed to add order");
                }
                break;
            }

            case JSON_MSG_BOOK_QUERY: {

                // Dynamically create or switch to the queried symbol
                if (!order_handler_create_book(parsed_msg.data.book_query.symbol)) {
                    LOG_ERROR("Failed to create/switch order book to symbol %s", 
                              parsed_msg.data.book_query.symbol);
                    break;
                }
                // Serialize and send order book
                char* book_json = order_handler_serialize_book();
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

        json_free_parsed_message(&parsed_msg);
    } else {
        LOG_ERROR("Failed to parse JSON message");
    }

    free(json_str);
}

int main(int argc, char* argv[]) {
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Quant Trading Server");
    
    // Initialize order handler
    if (!order_handler_init()) {
        LOG_ERROR("Failed to initialize order handler");
        return 1;
    }

    // Modify to require symbol specification
    if (!order_handler_create_book("AAPL")) {
        LOG_ERROR("Failed to create initial order book");
        return 1;
    }

    uint16_t port = DEFAULT_PORT;
    if (argc > 1) {
        port = (uint16_t)atoi(argv[1]);
    }

    // Setup signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

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
        order_handler_shutdown();
        return 1;
    }

    LOG_INFO("Trading server started on port %u. Press Ctrl+C to exit.", port);

    // Main server loop
    while (running) {
        ws_server_process(server);
        
        // Optional periodic tasks
        static time_t last_status = 0;
        time_t now = time(NULL);
        if (now - last_status >= 10) {
            OrderBook* book = order_handler_get_book();
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
    
    order_handler_shutdown();

    LOG_INFO("Server shutdown complete");
    return 0;
}
