#include "trading/engine/matcher.h"
#include "trading/protocol/messages.h"
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
    LOG_INFO("Received shutdown signal %d", sig);
    running = false;
    
    // Signal server to stop if it exists
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
static void on_client_message(WebSocketClient* client, const uint8_t* data, size_t len) {
    // Convert to null-terminated string
    char* json_str = malloc(len + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for message");
        return;
    }
    memcpy(json_str, data, len);
    json_str[len] = '\0';

    LOG_DEBUG("Received raw message from client (len=%zu): %s", len, json_str);

    // Parse the JSON message
    ParsedMessage parsed_msg;
    if (json_parse_message(json_str, &parsed_msg)) {
        LOG_DEBUG("Successfully parsed message, type=%d", parsed_msg.type);
        
        switch (parsed_msg.type) {
            case JSON_MSG_ORDER_ADD: {
                LOG_DEBUG("Processing order add: symbol=%s, price=%.2f, quantity=%u, is_buy=%d",
                         parsed_msg.data.order_add.symbol,
                         parsed_msg.data.order_add.order.price,
                         parsed_msg.data.order_add.order.quantity,
                         parsed_msg.data.order_add.order.is_buy);

                // Dynamically create or switch to the order's symbol
                if (!order_handler_create_book(parsed_msg.data.order_add.symbol)) {
                    LOG_ERROR("Failed to create/switch order book to symbol %s", 
                              parsed_msg.data.order_add.symbol);
                    break;
                }
                LOG_DEBUG("Successfully created/switched to order book for symbol %s",
                         parsed_msg.data.order_add.symbol);

                // Add order to the newly created/switched book
                OrderHandlingResult result = order_handler_add_order(&parsed_msg.data.order_add.order);
                
                if (result != ORDER_SUCCESS) {
                    LOG_ERROR("Failed to add order, result=%d", result);
                } else {
                    LOG_INFO("Successfully added order: id=%lu, symbol=%s, price=%.2f, quantity=%u, is_buy=%d",
                            parsed_msg.data.order_add.order.id,
                            parsed_msg.data.order_add.order.symbol,
                            parsed_msg.data.order_add.order.price,
                            parsed_msg.data.order_add.order.quantity,
                            parsed_msg.data.order_add.order.is_buy);
                    
                    // Get updated book state
                    OrderBook* book = order_handler_get_book();
                    if (book) {
                        LOG_DEBUG("Updated book state - Best Bid: %.2f, Best Ask: %.2f",
                                order_book_get_best_bid(book),
                                order_book_get_best_ask(book));
                    }
                }
                break;
            }

            case JSON_MSG_BOOK_QUERY: {
                LOG_DEBUG("Processing book query: symbol=%s", 
                         parsed_msg.data.book_query.symbol);
                         
                BookQueryConfig query_config;
                query_config.type = strlen(parsed_msg.data.book_query.symbol) > 0 
                    ? BOOK_QUERY_SYMBOL 
                    : BOOK_QUERY_ALL;
                
                // Copy symbol if specific symbol query
                if (query_config.type == BOOK_QUERY_SYMBOL) {
                    size_t symbol_len = strnlen(parsed_msg.data.book_query.symbol, 
                                                sizeof(query_config.symbol) - 1);
                    memcpy(query_config.symbol, 
                           parsed_msg.data.book_query.symbol, 
                           symbol_len);
                    query_config.symbol[symbol_len] = '\0';
                    LOG_DEBUG("Querying specific symbol: %s", query_config.symbol);
                } else {
                    LOG_DEBUG("Querying all symbols");
                }

                // Serialize book query result
                char* book_json = book_query_serialize(&query_config);
                if (book_json) {
                    LOG_INFO("Sending order book snapshot: %s", book_json);
                    ws_client_send(client, (const uint8_t*)book_json, strlen(book_json));
                    free(book_json);
                } else {
                    LOG_ERROR("Failed to serialize order book");
                }
                break;
            }

            default:
                LOG_WARN("Unhandled message type: %d", parsed_msg.type);
                break;
        }

        json_free_parsed_message(&parsed_msg);
    } else {
        LOG_ERROR("Failed to parse JSON message: %s", json_str);
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
        
        // Periodic tasks
        static time_t last_status = 0;
        time_t now = time(NULL);
        if (now - last_status >= 30) {
            OrderBook* book = order_handler_get_book();
            if (book) {
                LOG_INFO("Server status - Best Bid: %.2f, Best Ask: %.2f",
                        order_book_get_best_bid(book),
                        order_book_get_best_ask(book));
            }
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
