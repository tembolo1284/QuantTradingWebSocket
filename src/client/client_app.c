#include "trading/engine/order_book.h"
#include "net/websocket.h"
#include "net/websocket_io.h"
#include "utils/json_utils.h"
#include "utils/logging.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <stdint.h>
#include <errno.h>
#include <sys/select.h>

#define DEFAULT_HOST "quant_trading"
#define DEFAULT_PORT 8080
#define MAX_INPUT_SIZE 1024

static volatile bool running = true;
static atomic_uint_fast32_t order_counter = ATOMIC_VAR_INIT(0);

static void handle_signal(int sig) {
    LOG_INFO("Received shutdown signal %d", sig);
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

static void print_usage(void) {
    printf("\nAvailable commands:\n");
    printf("  order buy <price> <quantity> <symbol>   - Place buy order\n");
    printf("  order sell <price> <quantity> <symbol>  - Place sell order\n");
    printf("  cancel <order_id>              - Cancel order\n");
    printf("  book                           - Show order book\n");
    printf("  help                           - Show this help\n");
    printf("  quit                           - Exit client\n\n");
}

static void on_error(ErrorCode error, void* user_data) {
    (void)user_data;
    LOG_ERROR("WebSocket error: %s", ws_error_string(error));
    running = false;
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
            case JSON_MSG_ORDER_RESPONSE: {
                LOG_INFO("Order Response: success=%d, order_id=%lu, message=%s", 
                         parsed_msg.data.order_response.success,
                         parsed_msg.data.order_response.order_id,
                         parsed_msg.data.order_response.message);
                break;
            }
            default:
                LOG_WARN("Received unhandled message type: %d", parsed_msg.type);
                break;
        }

        json_free_parsed_message(&parsed_msg);
    } else {
        LOG_ERROR("Failed to parse JSON message: %s", json_str);
    }

    free(json_str);
}

static bool send_book_query(WebSocket* ws, const char* symbol) {
    ParsedMessage msg = {0};
    msg.type = JSON_MSG_BOOK_QUERY;

    if (symbol) {
        strncpy(msg.data.book_query.symbol, symbol, sizeof(msg.data.book_query.symbol) - 1);
        msg.data.book_query.type = BOOK_QUERY_SYMBOL;
        LOG_INFO("Requesting order book snapshot for %s", msg.data.book_query.symbol);
    } else {
        msg.data.book_query.type = BOOK_QUERY_ALL;
        msg.data.book_query.symbol[0] = '\0';
        LOG_INFO("Requesting order book snapshot for all symbols");
    }

    char* json_str = json_serialize_message(&msg);
    if (!json_str) return false;
    
    bool success = ws_send(ws, (const uint8_t*)json_str, strlen(json_str));
    free(json_str);
    return success;
}

static bool send_order(WebSocket* ws, bool is_buy, double price, uint32_t quantity, const char* symbol) {
    LOG_DEBUG("Creating order: %s %.2f %u %s", 
             is_buy ? "buy" : "sell", price, quantity, symbol);

    ParsedMessage msg = {0};
    msg.type = JSON_MSG_ORDER_ADD;
    msg.data.order_add.order.id = generate_order_id();
    msg.data.order_add.order.price = price;
    msg.data.order_add.order.quantity = quantity;
    msg.data.order_add.order.is_buy = is_buy;

    strncpy(msg.data.order_add.symbol, symbol, sizeof(msg.data.order_add.symbol) - 1);
    strncpy(msg.data.order_add.order.symbol, symbol, sizeof(msg.data.order_add.order.symbol) - 1);

    char* json_str = json_serialize_message(&msg);
    if (!json_str) {
        LOG_ERROR("Failed to serialize order");
        return false;
    }

    LOG_DEBUG("Sending order JSON: %s", json_str);
    bool success = ws_send(ws, (const uint8_t*)json_str, strlen(json_str));
    free(json_str);

    return success;
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
    if (!ws || !input) {
        LOG_ERROR("Invalid parameters");
        return;
    }

    LOG_INFO("Processing input: '%s'", input);
    
    // Skip whitespace
    while (isspace(*input)) input++;
    if (!*input) return;

    char command[32] = {0};
    if (sscanf(input, "%31s", command) != 1) {
        LOG_ERROR("Failed to parse command");
        return;
    }
    LOG_DEBUG("Parsed command: '%s'", command);

    if (strcmp(command, "help") == 0) {
        LOG_DEBUG("Showing help");
        print_usage();
    }
    else if (strcmp(command, "quit") == 0) {
        LOG_INFO("Quit command received");
        running = false;
    }
    else if (strcmp(command, "book") == 0) {
        LOG_DEBUG("Processing book command");
        char symbol[16] = {0};
        if (sscanf(input, "%*s %15s", symbol) == 1) {
            LOG_DEBUG("Querying book for symbol: %s", symbol);
            send_book_query(ws, symbol);
        } else {
            LOG_DEBUG("Querying full book");
            send_book_query(ws, NULL);
        }
    }
    else if (strcmp(command, "order") == 0) {
        char side[5] = {0};
        double price = 0.0;
        uint32_t quantity = 0;
        char symbol[16] = {0};

        int parsed = sscanf(input, "%*s %4s %lf %u %15s", side, &price, &quantity, symbol);
        LOG_DEBUG("Order parsing result: count=%d, side=%s, price=%.2f, qty=%u, symbol=%s",
                 parsed, side, price, quantity, symbol);

        if (parsed == 4) {
            if (strcmp(side, "buy") == 0 || strcmp(side, "sell") == 0) {
                bool is_buy = (strcmp(side, "buy") == 0);
                LOG_INFO("Sending %s order: %.2f %u %s", 
                        is_buy ? "buy" : "sell", price, quantity, symbol);
                send_order(ws, is_buy, price, quantity, symbol);
            } else {
                LOG_ERROR("Invalid order side '%s'", side);
            }
        } else {
            LOG_ERROR("Invalid order format (got %d fields)", parsed);
        }
    }
    else if (strcmp(command, "cancel") == 0) {
        uint64_t order_id;
        if (sscanf(input, "%*s %lu", &order_id) == 1) {
            LOG_DEBUG("Canceling order %lu", order_id);
            send_order_cancel(ws, order_id);
        } else {
            LOG_ERROR("Invalid cancel format");
        }
    }
    else {
        LOG_ERROR("Unknown command: '%s'", command);
    }
}

int main(int argc, char* argv[]) {
    // Extensive logging setup
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Market Client (PID: %d)", getpid());

    // Disable buffering for interactive use
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    // Parse command line arguments
    const char* host = argc >= 2 ? argv[1] : DEFAULT_HOST;
    uint16_t port = argc >= 3 ? (uint16_t)atoi(argv[2]) : DEFAULT_PORT;
    
    LOG_INFO("Connecting to host: %s, port: %u", host, port);

    // Set up signal handlers with enhanced logging
    if (signal(SIGINT, handle_signal) == SIG_ERR) {
        LOG_ERROR("Failed to set up SIGINT handler");
        return 1;
    }
    if (signal(SIGTERM, handle_signal) == SIG_ERR) {
        LOG_ERROR("Failed to set up SIGTERM handler");
        return 1;
    }

    // Create and connect WebSocket with detailed logging
    WebSocketCallbacks callbacks = {
        .on_message = on_message,
        .on_error = on_error,
        .user_data = NULL
    };

    LOG_DEBUG("Attempting to create WebSocket connection");
    WebSocket* ws = ws_create(host, port, &callbacks);
    if (!ws) {
        LOG_ERROR("Failed to connect to server at %s:%u", host, port);
        return 1;
    }

    LOG_INFO("Connected to trading server successfully");
    print_usage();

    // Main event loop with extensive logging
    char input[MAX_INPUT_SIZE];
    time_t last_message_time = time(NULL);

    while (running && ws_is_connected(ws)) {
        // Check if we should exit due to signal
        if (!running) {
            LOG_INFO("Interrupt received, initiating clean shutdown");
            break;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(ws->sock_fd, &readfds);

        struct timeval tv = {1, 0};  // 1 second timeout
        int maxfd = (ws->sock_fd > STDIN_FILENO) ? ws->sock_fd : STDIN_FILENO;

        int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);

        if (ready < 0) {
            if (errno == EINTR) {
                LOG_DEBUG("Select interrupted, checking exit condition");
                continue;
            }
            LOG_ERROR("Select error: %s (errno: %d)", strerror(errno), errno);
            break;
        }

        // Handle input with detailed logging
        if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\n")] = 0;
                LOG_DEBUG("User input received: '%s'", input);
                process_user_input(ws, input);
                last_message_time = time(NULL);
            }
        }

        // Handle socket events with detailed logging
        if (ready > 0 && FD_ISSET(ws->sock_fd, &readfds)) {
            LOG_DEBUG("Socket ready for processing");
            ws_process(ws);
            last_message_time = time(NULL);
        }

        // Connection timeout monitoring
        if (time(NULL) - last_message_time > 30) {
            if (!ws_is_connected(ws)) {
                LOG_ERROR("Connection to server lost after 30 seconds of inactivity");
                break;
            }
        }

        // Avoid busy loop
        if (ready == 0) {
            usleep(10000);  // 10ms sleep on timeout
        }
    }

    LOG_INFO("Initiating client shutdown sequence");
    if (ws) {
        LOG_DEBUG("Closing WebSocket connection");
        ws_close(ws);
        ws = NULL;
    }
    LOG_INFO("Market client shutdown complete");

    return 0;
}
