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
#include <time.h>

#define DEFAULT_PORT 8080
#define STATUS_UPDATE_INTERVAL 30  // seconds

static volatile bool running = true;
static WebSocketServer* server = NULL;
static time_t server_start_time;

static void print_server_status(void) {
    time_t now = time(NULL);
    double uptime = difftime(now, server_start_time);
    
    size_t max_books = order_handler_get_active_book_count();
    OrderBook** books = malloc(sizeof(OrderBook*) * max_books);
    size_t active_books = 0;
    
    if (books) {
        active_books = order_handler_get_all_books(books, max_books);
    }

    LOG_INFO("=== Server Status Report ===");
    LOG_INFO("Server Uptime: %.2f hours (%.0f seconds)", uptime / 3600.0, uptime);
    LOG_INFO("Status: Active and accepting connections");
    LOG_INFO("Active Order Books: %zu", active_books);
    
    if (books) {
        for (size_t i = 0; i < active_books; i++) {
            if (!books[i]) continue;
            
            double best_bid = order_book_get_best_bid(books[i]);
            double best_ask = order_book_get_best_ask(books[i]);
            size_t total_orders = order_book_get_order_count(books[i]);
            
            LOG_INFO("Order Book: %s", order_book_get_symbol(books[i]));
            LOG_INFO("  - Total Orders: %zu", total_orders);
            LOG_INFO("  - Best Bid: %.2f", best_bid);
            LOG_INFO("  - Best Ask: %.2f", best_ask);
            LOG_INFO("  - Spread: %.2f", best_ask > 0 && best_bid > 0 ? best_ask - best_bid : 0.0);
        }
        free(books);
    }
    LOG_INFO("=========================");
}

static void on_trade_executed(const Trade* trade, void* user_data) {
    (void)user_data;

    char* trade_json = trade_notification_serialize(trade);
    if (trade_json) {
        LOG_INFO("Trade executed: %s at %.2f (Quantity: %u)", 
                trade->symbol, trade->price, trade->quantity);
        ws_server_broadcast(server, (const uint8_t*)trade_json, strlen(trade_json));
        free(trade_json);
    }
}

static void handle_signal(int sig) {
    LOG_INFO("Received shutdown signal %d", sig);
    running = false;

    if (server) {
        ws_server_request_shutdown(server);
    }
}

static void on_client_connect(WebSocketClient* client) {
    LOG_INFO("New client connected - ID: %u", client->client_id);
    
    // Send current market snapshot to new client
    BookQueryConfig query_config = {
        .type = BOOK_QUERY_ALL
    };
    
    char* snapshot = book_query_serialize(&query_config);
    if (snapshot) {
        ws_client_send(client, (const uint8_t*)snapshot, strlen(snapshot));
        free(snapshot);
    }
}

static void on_client_disconnect(WebSocketClient* client) {
    LOG_INFO("Client disconnected - ID: %u", client->client_id);
}

static void on_client_message(WebSocketClient* client, const uint8_t* data, size_t len) {
    char* json_str = malloc(len + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for message");
        return;
    }
    memcpy(json_str, data, len);
    json_str[len] = '\0';

    LOG_DEBUG("Received message from client %u (len=%zu): %s", 
             client->client_id, len, json_str);

    ParsedMessage parsed_msg;
    if (json_parse_message(json_str, &parsed_msg)) {
        LOG_DEBUG("Successfully parsed message from client %u, type=%d", 
                 client->client_id, parsed_msg.type);

        switch (parsed_msg.type) {
            case JSON_MSG_ORDER_ADD: {
                LOG_DEBUG("Client %u: Processing order add: symbol=%s, price=%.2f, quantity=%u, is_buy=%d",
                         client->client_id,
                         parsed_msg.data.order_add.symbol,
                         parsed_msg.data.order_add.order.price,
                         parsed_msg.data.order_add.order.quantity,
                         parsed_msg.data.order_add.order.is_buy);

                if (!order_handler_create_book(parsed_msg.data.order_add.symbol)) {
                    LOG_ERROR("Failed to create/switch order book to symbol %s",
                              parsed_msg.data.order_add.symbol);
                    break;
                }

                OrderHandlingResult result = order_handler_add_order(&parsed_msg.data.order_add.order);

                char* response = order_response_serialize(
                    parsed_msg.data.order_add.order.id,
                    result == ORDER_SUCCESS,
                    result == ORDER_SUCCESS ? "Order accepted" : "Order rejected"
                );

                if (response) {
                    ws_client_send(client, (const uint8_t*)response, strlen(response));
                    free(response);
                }

                // Trigger immediate status update after order
                print_server_status();
                break;
            }

            case JSON_MSG_ORDER_CANCEL: {
                LOG_DEBUG("Client %u: Processing cancel for order %lu",
                         client->client_id,
                         parsed_msg.data.order_cancel.order_id);

                CancelResult result = order_book_cancel(
                    order_handler_get_book(),
                    parsed_msg.data.order_cancel.order_id
                );

                char* response = cancel_response_serialize(
                    result,
                    parsed_msg.data.order_cancel.order_id
                );

                if (response) {
                    ws_client_send(client, (const uint8_t*)response, strlen(response));
                    free(response);
                }

                // Trigger immediate status update after cancellation
                print_server_status();
                break;
            }

            case JSON_MSG_BOOK_QUERY: {
                LOG_DEBUG("Client %u: Processing book query: symbol=%s",
                         client->client_id,
                         parsed_msg.data.book_query.symbol);

                BookQueryConfig query_config;
                query_config.type = strlen(parsed_msg.data.book_query.symbol) > 0
                    ? BOOK_QUERY_SYMBOL
                    : BOOK_QUERY_ALL;

                if (query_config.type == BOOK_QUERY_SYMBOL) {
                    size_t symbol_len = strnlen(parsed_msg.data.book_query.symbol,
                                                sizeof(query_config.symbol) - 1);
                    memcpy(query_config.symbol,
                           parsed_msg.data.book_query.symbol,
                           symbol_len);
                    query_config.symbol[symbol_len] = '\0';
                }

                char* book_json = book_query_serialize(&query_config);
                if (book_json) {
                    ws_client_send(client, (const uint8_t*)book_json, strlen(book_json));
                    free(book_json);
                }
                break;
            }

            default:
                LOG_WARN("Client %u: Unhandled message type: %d", 
                        client->client_id, parsed_msg.type);
                break;
        }

        json_free_parsed_message(&parsed_msg);
    } else {
        LOG_ERROR("Client %u: Failed to parse message: %s", 
                 client->client_id, json_str);
    }

    free(json_str);
}

int main(int argc, char* argv[]) {
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Quant Trading Server");

    server_start_time = time(NULL);

    if (!order_handler_init()) {
        LOG_ERROR("Failed to initialize order handler");
        return 1;
    }

    uint16_t port = DEFAULT_PORT;
    if (argc > 1) {
        port = (uint16_t)atoi(argv[1]);
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_ERROR("Failed to set up signal handlers");
        return 1;
    }

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

    // Set up trade notification callback for all books
    OrderBook** books = malloc(sizeof(OrderBook*) * MAX_SYMBOLS);
    if (books) {
        size_t book_count = order_handler_get_all_books(books, MAX_SYMBOLS);
        for (size_t i = 0; i < book_count; i++) {
            order_book_set_trade_callback(books[i], on_trade_executed, NULL);
        }
        free(books);
    }

    LOG_INFO("Trading server started on port %u. Press Ctrl+C to exit.", port);
    // print_server_status();  // Initial status

    while (running) {
        ws_server_process(server);

        static time_t last_status = 0;
        time_t now = time(NULL);
        if (now - last_status >= STATUS_UPDATE_INTERVAL) {
            print_server_status();
            last_status = now;
        }

        usleep(10000);  // 10ms sleep
    }

    LOG_INFO("Shutting down server...");

    if (server) {
        ws_server_destroy(server);
        server = NULL;
    }

    order_handler_shutdown();

    LOG_INFO("Server shutdown complete");
    return 0;
}
