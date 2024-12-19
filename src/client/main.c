#include "trading/order_book.h"
#include "net/websocket.h"
#include "utils/json_utils.h"
#include "utils/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT 8080

static volatile bool running = true;
static OrderBook* local_book = NULL;  // Local copy of order book
static uint64_t next_order_id = 1;

static void handle_signal(int sig) {
    (void)sig;
    LOG_INFO("Received shutdown signal");
    running = false;
}

static void print_order_book(const BookSymbol* symbol) {
    time_t now;
    time(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    printf("\nOrder Book for %s\n", symbol->symbol);
    printf("================================================================================\n\n");

    // Print Buy Orders
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

    // Print Sell Orders
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
    
    // Convert to null-terminated string
    char* json_str = malloc(len + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for message");
        return;
    }
    memcpy(json_str, data, len);
    json_str[len] = '\0';

    LOG_DEBUG("Received raw message (len=%zu): %s", len, json_str);

    // Parse JSON message
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
    running = false;  // Disconnect on error
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
        // Query specific symbol
        size_t symbol_len = strnlen(symbol, sizeof(msg.data.book_query.symbol) - 1);
        memcpy(msg.data.book_query.symbol, symbol, symbol_len);
        msg.data.book_query.symbol[symbol_len] = '\0';
        msg.data.book_query.type = BOOK_QUERY_SYMBOL;
        LOG_INFO("Requesting order book snapshot for %s", msg.data.book_query.symbol);
    } else {
        // Query all symbols
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
    LOG_DEBUG("Preparing to send order: is_buy=%d, price=%.2f, quantity=%u, symbol=%s",
             is_buy, price, quantity, symbol);
             
    ParsedMessage msg = {0};
    msg.type = JSON_MSG_ORDER_ADD;
    
    // Safely copy symbol with explicit null-termination
    size_t symbol_len = strnlen(symbol, sizeof(msg.data.order_add.symbol) - 1);
    memcpy(msg.data.order_add.symbol, symbol, symbol_len);
    msg.data.order_add.symbol[symbol_len] = '\0';

    msg.data.order_add.order.id = next_order_id++;
    msg.data.order_add.order.price = price;
    msg.data.order_add.order.quantity = quantity;
    msg.data.order_add.order.is_buy = is_buy;
    
    // Safely copy symbol to order
    symbol_len = strnlen(symbol, sizeof(msg.data.order_add.order.symbol) - 1);
    memcpy(msg.data.order_add.order.symbol, symbol, symbol_len);
    msg.data.order_add.order.symbol[symbol_len] = '\0';

    LOG_DEBUG("Order message prepared: order_id=%lu, symbol=%s", 
              msg.data.order_add.order.id,
              msg.data.order_add.symbol);

    char* json_str = json_serialize_message(&msg);
    if (json_str) {
        LOG_DEBUG("Sending order JSON: %s", json_str);
        int result = ws_send(ws, (const uint8_t*)json_str, strlen(json_str));
        if (result < 0) {
            LOG_ERROR("Failed to send order message: %d", result);
        } else {
            LOG_DEBUG("Successfully sent order message (%d bytes)", result);
        }
        free(json_str);
    } else {
        LOG_ERROR("Failed to serialize order message");
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

static void process_user_input(WebSocket* ws) {
    char buffer[256];
    char command[32];
    
    printf("Enter command (type 'help' for usage): ");
    fflush(stdout);
    
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return;
    }
    
    // Remove trailing newline
    buffer[strcspn(buffer, "\n")] = 0;
    
    LOG_DEBUG("Received command: '%s'", buffer);  // Debug log the raw command
    
    if (sscanf(buffer, "%31s", command) != 1) {
        LOG_ERROR("Failed to parse command");
        return;
    }
    
    LOG_DEBUG("Parsed command: '%s'", command);  // Debug log the parsed command
    
    if (strcmp(command, "help") == 0) {
        print_usage();
    }
    else if (strcmp(command, "quit") == 0) {
        running = false;
    }
    else if (strcmp(command, "book") == 0) {
        char symbol[16] = {0};

        if (sscanf(buffer, "%*s %15s", symbol) == 1) {
            send_book_query(ws, symbol);
        } else {
            send_book_query(ws, NULL);
        }
    }
    else if (strcmp(command, "order") == 0) {
        char side[5] = {0};
        double price = 0.0;
        uint32_t quantity = 0;
        char symbol[16] = {0};
        
        int parsed = sscanf(buffer, "%*s %4s %lf %u %15s", side, &price, &quantity, symbol);
        LOG_DEBUG("Order parsing result: count=%d, side='%s', price=%.2f, quantity=%u, symbol='%s'",
                 parsed, side, price, quantity, symbol);
                 
        if (parsed == 4) {
            if (strcmp(side, "buy") == 0 || strcmp(side, "sell") == 0) {
                bool is_buy = (strcmp(side, "buy") == 0);
                LOG_INFO("Sending %s order: %.2f %u %s", 
                        is_buy ? "buy" : "sell", price, quantity, symbol);
                send_order(ws, is_buy, price, quantity, symbol);
            } else {
                LOG_ERROR("Invalid order side '%s'. Must be 'buy' or 'sell'", side);
            }
        } else {
            LOG_ERROR("Invalid order format (parsed %d fields). Use: order <buy|sell> <price> <quantity> <symbol>", 
                     parsed);
        }
    }
    else if (strcmp(command, "cancel") == 0) {
        uint64_t order_id;
        if (sscanf(buffer, "%*s %lu", &order_id) == 1) {
            send_order_cancel(ws, order_id);
        } else {
            LOG_ERROR("Invalid cancel format. Use: cancel <order_id>");
        }
    }
    else {
        LOG_ERROR("Unknown command '%s'. Type 'help' for usage.", command);
    }
}

int main(int argc, char* argv[]) {
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Market Client");

    // Parse command line arguments
    const char* host = DEFAULT_HOST;
    uint16_t port = DEFAULT_PORT;

    if (argc > 1) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s [host port]\n", argv[0]);
            fprintf(stderr, "Or run without arguments to use defaults: %s:%d\n", 
                    DEFAULT_HOST, DEFAULT_PORT);
            return 1;
        }
        host = argv[1];
        port = (uint16_t)atoi(argv[2]);
    }

    // Setup signal handler
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create local order book
    local_book = order_book_create("AAPL");
    if (!local_book) {
        LOG_ERROR("Failed to create local order book");
        return 1;
    }

    // Setup WebSocket callbacks
    WebSocketCallbacks callbacks = {
        .on_message = on_message,
        .on_error = on_error,
        .user_data = local_book
    };

    // Connect to server
    LOG_INFO("Connecting to server at %s:%u", host, port);
    WebSocket* ws = ws_create(host, port, &callbacks);
    if (!ws) {
        LOG_ERROR("Failed to connect to server");
        order_book_destroy(local_book);
        return 1;
    }

    LOG_INFO("Connected to trading server successfully");
    print_usage();

    // Main event loop
    while (running) {
        // Process server messages
        ws_process(ws);

        // Check for user input
        fd_set read_fds;
        struct timeval tv = {0, 100000};  // 100ms timeout
        
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        if (select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv) > 0) {
            process_user_input(ws);
        }
    }

    // Cleanup
    LOG_INFO("Shutting down client...");
    ws_close(ws);
    order_book_destroy(local_book);
    LOG_INFO("Client shutdown complete");

    return 0;
}
