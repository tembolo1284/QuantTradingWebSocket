#include "trading/order_book.h"
#include "net/websocket.h"
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

static void handle_signal(int sig) {
    (void)sig;
    LOG_INFO("Received shutdown signal");
    running = false;
}

// Handle market data updates from server
static void on_message(const uint8_t* data, size_t len, void* user_data) {
    LOG_DEBUG("Received message from server, length: %zu bytes", len);
    (void)user_data;    
    // TODO: Parse server message and update local order book
    if (len > 0) {
        char preview[64] = {0};
        size_t preview_len = len < sizeof(preview) - 1 ? len : sizeof(preview) - 1;
        memcpy(preview, data, preview_len);
        LOG_INFO("Server message: %s", preview);
    }

    // Display current order book state
    if (local_book) {
        double best_bid = order_book_get_best_bid(local_book);
        double best_ask = order_book_get_best_ask(local_book);
        LOG_INFO("Order Book - Best Bid: %.2f, Best Ask: %.2f", best_bid, best_ask);
    }
}

static void on_error(ErrorCode error, void* user_data) {
    (void)user_data;
    LOG_ERROR("WebSocket error: %s", ws_error_string(error));
    running = false;  // Disconnect on error
}

static void print_usage(void) {
    printf("\nAvailable commands:\n");
    printf("  order buy <price> <quantity>   - Place buy order\n");
    printf("  order sell <price> <quantity>  - Place sell order\n");
    printf("  cancel <order_id>              - Cancel order\n");
    printf("  book                           - Show order book\n");
    printf("  help                           - Show this help\n");
    printf("  quit                           - Exit client\n\n");
}

static void process_user_input(WebSocket* ws) {
    char buffer[256];
    char command[32];
    
    printf("Enter command (type 'help' for usage): ");
    fflush(stdout);
    
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return;
    }
    
    if (sscanf(buffer, "%31s", command) != 1) {
        return;
    }
    
    if (strcmp(command, "help") == 0) {
        print_usage();
    }
    else if (strcmp(command, "quit") == 0) {
        running = false;
    }
    else if (strcmp(command, "order") == 0) {
        char side[5];
        double price;
        int quantity;
        
        if (sscanf(buffer, "%*s %4s %lf %d", side, &price, &quantity) == 3) {
            bool is_buy = (strcmp(side, "buy") == 0);
            
            // Format and send order to server
            char order_msg[128];
            snprintf(order_msg, sizeof(order_msg), 
                    "{\"type\":\"order\",\"side\":\"%s\",\"price\":%.2f,\"quantity\":%d}",
                    is_buy ? "buy" : "sell", price, quantity);
                    
            LOG_INFO("Sending order: %s", order_msg);
            ws_send(ws, (const uint8_t*)order_msg, strlen(order_msg));
        }
        else {
            LOG_ERROR("Invalid order format. Use: order <buy|sell> <price> <quantity>");
        }
    }
    else if (strcmp(command, "cancel") == 0) {
        uint64_t order_id;
        if (sscanf(buffer, "%*s %lu", &order_id) == 1) {
            char cancel_msg[64];
            snprintf(cancel_msg, sizeof(cancel_msg),
                    "{\"type\":\"cancel\",\"order_id\":%lu}", order_id);
                    
            LOG_INFO("Sending cancel request: %s", cancel_msg);
            ws_send(ws, (const uint8_t*)cancel_msg, strlen(cancel_msg));
        }
        else {
            LOG_ERROR("Invalid cancel format. Use: cancel <order_id>");
        }
    }
    else if (strcmp(command, "book") == 0) {
        char book_msg[] = "{\"type\":\"book_request\"}";
        LOG_INFO("Requesting order book snapshot");
        ws_send(ws, (const uint8_t*)book_msg, strlen(book_msg));
    }
    else {
        LOG_ERROR("Unknown command. Type 'help' for usage.");
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
