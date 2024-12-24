#include "trading/engine/order_book.h"
#include "net/websocket.h"
#include "net/websocket_io.h"
#include "utils/json_utils.h"
#include "utils/logging.h"
#include "client/client_helper.h"
#include "common/types.h"
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
// static atomic_uint_fast32_t order_counter = ATOMIC_VAR_INIT(0);

static void handle_signal(int sig) {
    LOG_INFO("Received shutdown signal %d", sig);
    running = false;
}

static void on_error(ErrorCode error, void* user_data) {
    LOG_ERROR("WebSocket error: %s", error_code_to_string(error));
    running = false;
    
    switch (error) {
        case ERROR_WS_SEND_FAILED:
            LOG_ERROR("WebSocket send failed");
            break;
        case ERROR_WS_CONNECTION_FAILED:
            LOG_ERROR("WebSocket connection failed");
            break;
        case ERROR_WS_HANDSHAKE_FAILED:
            LOG_ERROR("WebSocket handshake failed");
            break;
        case ERROR_WS_INVALID_FRAME:
            LOG_ERROR("Invalid WebSocket frame");
            break;
        default:
            break;
    }
}

static void on_message(const uint8_t* data, size_t len, void* user_data) {
    (void)user_data;

    if (!data || len == 0) {
        LOG_ERROR("Received empty or NULL message");
        return;
    }

    char* json_str = malloc(len + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for message");
        return;
    }
    memcpy(json_str, data, len);
    json_str[len] = '\0';

    LOG_DEBUG("Received raw message (len=%zu): %s", len, json_str);

    ParsedMessage parsed_msg;
    memset(&parsed_msg, 0, sizeof(ParsedMessage));

    if (json_parse_message(json_str, &parsed_msg)) {
        switch (parsed_msg.type) {
            case JSON_MSG_BOOK_RESPONSE: {
                LOG_DEBUG("Processing book response with %zu symbols", 
                          parsed_msg.data.book_response.symbols_count);

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
    set_log_level(LOG_DEBUG);
    LOG_INFO("Starting Market Client (PID: %d)", getpid());

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    const char* host = argc >= 2 ? argv[1] : DEFAULT_HOST;
    uint16_t port = argc >= 3 ? (uint16_t)atoi(argv[2]) : DEFAULT_PORT;
    
    LOG_INFO("Connecting to host: %s, port: %u", host, port);

    if (signal(SIGINT, handle_signal) == SIG_ERR ||
        signal(SIGTERM, handle_signal) == SIG_ERR) {
        LOG_ERROR("Failed to set up signal handlers");
        return 1;
    }

    WebSocketCallbacks callbacks = {
        .on_message = on_message,
        .on_error = on_error,
        .user_data = NULL
    };

    WebSocket* ws = ws_create(host, port, &callbacks);
    if (!ws) {
        LOG_ERROR("Failed to connect to server at %s:%u", host, port);
        return 1;
    }

    LOG_INFO("Connected to trading server successfully");
    print_usage();

    char input[MAX_INPUT_SIZE];
    time_t last_message_time = time(NULL);

    while (running && ws_is_connected(ws)) {
        if (!running || !ws) {
            LOG_INFO("Interrupt or connection lost, initiating shutdown");
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
                LOG_DEBUG("Select interrupted, continuing");
                continue;
            }
            LOG_ERROR("Select error: %s (errno: %d)", strerror(errno), errno);
            break;
        }

        if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\n")] = 0;
                LOG_DEBUG("User input received: '%s'", input);
                process_user_input(ws, input);
                last_message_time = time(NULL);
            }
        }

        if (ready > 0 && FD_ISSET(ws->sock_fd, &readfds)) {
            ws_process(ws);
            last_message_time = time(NULL);
        }

        if (time(NULL) - last_message_time > 30) {
            if (!ws_is_connected(ws)) {
                LOG_ERROR("Connection to server lost");
                break;
            }
        }

        if (ready == 0) {
            usleep(10000);  // 10ms sleep
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
