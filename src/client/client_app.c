#include "trading/engine/order_book.h"
#include "net/websocket.h"
#include "net/websocket_io.h"
#include "utils/json_utils.h"
#include "utils/logging.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <stdint.h>
#include <poll.h>

#define DEFAULT_HOST "quant_trading"
#define DEFAULT_PORT 8080
#define MAX_INPUT_SIZE 1024

static volatile bool running = true;
static atomic_uint_fast32_t order_counter = ATOMIC_VAR_INIT(0);

static void handle_signal(int sig) {
    (void)sig;
    LOG_INFO("Received shutdown signal");
    running = false;
}

static uint64_t generate_order_id(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t counter = atomic_fetch_add(&order_counter, 1);
    uint32_t nano_counter = (uint32_t)(ts.tv_nsec / 1000) ^ counter;
    uint64_t id = ((uint64_t)ts.tv_sec << 32) | nano_counter;
    LOG_DEBUG("Generated order ID: %lu (timestamp: %lu, counter: %u)",
              id, (uint64_t)ts.tv_sec, counter);
    return id;
}

static void print_order_book(const BookSymbol* symbol) {
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

static void on_message(const uint8_t* data, size_t len, void* user_data) {
    (void)user_data;

    char* json_str = malloc(len + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for message");
        return;
    }
    memcpy(json_str, data, len);
    json_str[len] = '\0';

    LOG_DEBUG("Received raw message (len=%zu): %s", len, json_str);

    ParsedMessage parsed_msg;
    if (json_parse_message(json_str, &parsed_msg)) {
        switch (parsed_msg.type) {
            case JSON_MSG_BOOK_RESPONSE: {
                if (parsed_msg.data.book_response.symbols_count == 0) {
                    printf("\nNo orders in the book\n\n");
                } else {
                    for (size_t i = 0; i < parsed_msg.data.book_response.symbols_count; i++) {
                        print_order_book(&parsed_msg.data.book_response.symbols[i]);
                    }
                }
                break;
            }
            default:
                LOG_DEBUG("Received unhandled message type: %d", parsed_msg.type);
                break;
        }

        json_free_parsed_message(&parsed_msg);
    } else {
        LOG_ERROR("Failed to parse JSON message: %s", json_str);
    }

    free(json_str);
}

static void on_error(ErrorCode error, void* user_data) {
    (void)user_data;
    LOG_ERROR("WebSocket error: %s", ws_error_string(error));
    running = false;
}

static void print_usage(void) {
    printf("\nAvailable commands:\n");
    printf("  order buy <price> <quantity> <symbol>   - Place buy order\n");
    printf("  order sell <price> <quantity> <symbol>  - Place sell order\n");
    printf("  cancel <order_id>              - Cancel order\n");
    printf("  book                           - Show order book\n");
    printf("  help                           - Show this help\n");
    printf("  quit                           - Exit client\n\n");
}

static void send_book_query(WebSocket* ws, const char* symbol) {
    ParsedMessage msg = {0};
    msg.type = JSON_MSG_BOOK_QUERY;

    if (symbol) {
        size_t symbol_len = strnlen(symbol, sizeof(msg.data.book_query.symbol) - 1);
        memcpy(msg.data.book_query.symbol, symbol, symbol_len);
        msg.data.book_query.symbol[symbol_len] = '\0';
        msg.data.book_query.type = BOOK_QUERY_SYMBOL;
        LOG_INFO("Requesting order book snapshot for %s", msg.data.book_query.symbol);
    } else {
        msg.data.book_query.type = BOOK_QUERY_ALL;
        msg.data.book_query.symbol[0] = '\0';
        LOG_INFO("Requesting order book snapshot for all symbols");
    }

    char* json_str = json_serialize_message(&msg);
    if (json_str) {
        ws_send(ws, (const uint8_t*)json_str, strlen(json_str));
        free(json_str);
    }
}

static void send_order(WebSocket* ws, bool is_buy, double price, uint32_t quantity, const char* symbol) {
    ParsedMessage msg = {0};
    msg.type = JSON_MSG_ORDER_ADD;
    msg.data.order_add.order.id = generate_order_id();
    msg.data.order_add.order.price = price;
    msg.data.order_add.order.quantity = quantity;
    msg.data.order_add.order.is_buy = is_buy;
    
    strncpy(msg.data.order_add.symbol, symbol, sizeof(msg.data.order_add.symbol) - 1);
    strncpy(msg.data.order_add.order.symbol, symbol, sizeof(msg.data.order_add.order.symbol) - 1);

    char* json_str = json_serialize_message(&msg);
    if (json_str) {
        LOG_DEBUG("Sending order JSON: %s", json_str);
        if (!ws_send(ws, (const uint8_t*)json_str, strlen(json_str))) {
            LOG_ERROR("Failed to send order");
        }
        free(json_str);
    }
}

static void send_order_cancel(WebSocket* ws, uint64_t order_id) {
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

static void process_user_input(WebSocket* ws, const char* input) {
    char command[32] = {0};
    
    if (!input || strlen(input) == 0 || sscanf(input, "%31s", command) != 1) {
        return;
    }

    LOG_DEBUG("Processing command: '%s'", command);
    bool command_processed = false;

    if (strcmp(command, "help") == 0) {
        print_usage();
        command_processed = true;
    }
    else if (strcmp(command, "quit") == 0) {
        running = false;
        command_processed = true;
    }
    else if (strcmp(command, "book") == 0) {
        char symbol[16] = {0};
        if (sscanf(input, "%*s %15s", symbol) == 1) {
            send_book_query(ws, symbol);
        } else {
            send_book_query(ws, NULL);
        }
        command_processed = true;
    }
    else if (strcmp(command, "order") == 0) {
        char side[5] = {0};
        double price = 0.0;
        uint32_t quantity = 0;
        char symbol[16] = {0};

        if (sscanf(input, "%*s %4s %lf %u %15s", side, &price, &quantity, symbol) == 4) {
            if (strcmp(side, "buy") == 0 || strcmp(side, "sell") == 0) {
                bool is_buy = (strcmp(side, "buy") == 0);
                send_order(ws, is_buy, price, quantity, symbol);
                command_processed = true;
            }
        }
    }
    else if (strcmp(command, "cancel") == 0) {
        uint64_t order_id;
        if (sscanf(input, "%*s %lu", &order_id) == 1) {
            send_order_cancel(ws, order_id);
            command_processed = true;
        }
    }
    
    if (!command_processed) {
        LOG_ERROR("Unknown or malformed command '%s'. Type 'help' for usage.", command);
    }
    
    // Force immediate processing after command
    ws_process(ws);
}

int main(int argc, char* argv[]) {
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Market Client");

    const char* host = argc >= 2 ? argv[1] : DEFAULT_HOST;
    uint16_t port = argc >= 3 ? (uint16_t)atoi(argv[2]) : DEFAULT_PORT;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    WebSocketCallbacks callbacks = {
        .on_message = on_message,
        .on_error = on_error,
        .user_data = NULL
    };

    WebSocket* ws = ws_create(host, port, &callbacks);
    if (!ws) {
        LOG_ERROR("Failed to connect to server");
        return 1;
    }

    LOG_INFO("Connected to trading server successfully");
    print_usage();

    struct pollfd fds[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = ws->sock_fd, .events = POLLIN }
    };
    
    char input[MAX_INPUT_SIZE];
    bool clean_shutdown = false;

    while (running && ws_is_connected(ws)) {
        int ret = poll(fds, 2, 100);  // 100ms timeout
        
        if (ret < 0) {
            if (errno == EINTR) {
                if (!running) {
                    clean_shutdown = true;
                    break;
                }
                continue;
            }
            LOG_ERROR("Poll error: %s", strerror(errno));
            break;
        }
        
        // Check for user input
        if (ret > 0 && (fds[0].revents & POLLIN)) {
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\n")] = 0;
                process_user_input(ws, input);
                // Immediately process any websocket messages after sending command
                ws_process(ws);
            }
        }
        
        // Check for websocket messages
        if (running && ws_is_connected(ws)) {
            if (ret > 0 && (fds[1].revents & (POLLIN | POLLHUP))) {
                ws_process(ws);
            } else if (ret == 0) {
                // Regular polling even without events
                ws_process(ws);
            }
        }
    }

    LOG_INFO("Shutting down client...");
    
    if (clean_shutdown && ws && ws_is_connected(ws)) {
        LOG_DEBUG("Performing clean shutdown");
        ws_close(ws);
    }
    
    LOG_INFO("Client shutdown complete");
    return 0;
}
