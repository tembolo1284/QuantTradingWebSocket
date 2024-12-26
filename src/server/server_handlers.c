#include "server/server_handlers.h"
#include "protocol/json_protocol.h"
#include "protocol/message_types.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_SYMBOLS 100

struct ServerHandlers {
    pthread_t* worker_threads;
    int thread_count;
    bool running;
    
    // Message queue
    char** message_queue;
    WSClient** client_queue;
    int queue_size;
    int queue_head;
    int queue_tail;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    
    // Order books
    OrderBook* books[MAX_SYMBOLS];
    char symbols[MAX_SYMBOLS][16];
    int book_count;
    pthread_rwlock_t books_lock;
};

// Message handler lookup table
static const struct {
    int msg_type;
    MessageHandler handler;
} message_handlers[] = {
    {MSG_PLACE_ORDER, handle_place_order},
    {MSG_CANCEL_ORDER, handle_cancel_order},
    {MSG_REQUEST_BOOK, handle_book_request}
};

static void collect_orders(Order* order, void* user_data) {
    BookSnapshot* snap = (BookSnapshot*)user_data;
    if (order->is_canceled || order->remaining_quantity <= 0) {
        return;
    }

    if (order_is_buy_order(order)) {
        if ((size_t)snap->num_bids < snap->max_orders) {
            snap->bid_prices[snap->num_bids] = order_get_price(order);
            snap->bid_quantities[snap->num_bids] = order_get_remaining_quantity(order);
            snap->num_bids++;
        }
    } else {
        if ((size_t)snap->num_asks < snap->max_orders) {
            snap->ask_prices[snap->num_asks] = order_get_price(order);
            snap->ask_quantities[snap->num_asks] = order_get_remaining_quantity(order);
            snap->num_asks++;
        }
    }
}

// Helper Functions
char* dequeue_message(ServerHandlers* handlers, WSClient** client) {
    pthread_mutex_lock(&handlers->queue_lock);
    
    while (handlers->queue_head == handlers->queue_tail && handlers->running) {
        pthread_cond_wait(&handlers->queue_cond, &handlers->queue_lock);
    }
    
    if (!handlers->running) {
        pthread_mutex_unlock(&handlers->queue_lock);
        return NULL;
    }
    
    char* message = handlers->message_queue[handlers->queue_head];
    *client = handlers->client_queue[handlers->queue_head];
    handlers->queue_head = (handlers->queue_head + 1) % handlers->queue_size;
    
    pthread_mutex_unlock(&handlers->queue_lock);
    return message;
}

int send_error_response(WSClient* client, const char* error_msg, char* response) {
    snprintf(response, 1024, 
            "{\"type\": %d, \"status\": \"failed\", \"reason\": \"%s\"}",
            MSG_ERROR, error_msg);
    return ws_server_send(client, response, strlen(response));
}

// Message Handlers
int handle_place_order(ServerHandlers* handlers, WSClient* client, const cJSON* root, char* response) {
    OrderMessage order;
    if (!parse_order_message(cJSON_Print(root), &order)) {
        return send_error_response(client, "Invalid order format", response);
    }

    LOG_INFO("Processing order: %s %s %.2f x %d",
             order.order_id, order.symbol, order.price, order.quantity);

    pthread_rwlock_rdlock(&handlers->books_lock);
    bool order_placed = false;

    // Find or create order book
    OrderBook* book = NULL;
    for (int i = 0; i < handlers->book_count; i++) {
        if (strcmp(handlers->symbols[i], order.symbol) == 0) {
            book = handlers->books[i];
            break;
        }
    }

    if (!book) {
        book = order_book_create();
        if (book) {
            handlers->books[handlers->book_count] = book;
            strncpy(handlers->symbols[handlers->book_count], order.symbol,
                    sizeof(handlers->symbols[0]) - 1);
            handlers->book_count++;
            LOG_INFO("Created new order book for symbol %s", order.symbol);
        }
    }

    if (book) {
        struct Order* new_order = order_create(
            order.order_id, order.trader_id, order.symbol,
            order.price, order.quantity, order.is_buy
        );

        if (new_order) {
            order_book_add_order(book, new_order);
            order_placed = true;

            // Get current timestamp
            time_t now;
            time(&now);
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

            // Send formatted confirmation
            snprintf(response, 1024,
                "{\n"
                "    \"type\": %d,\n"
                "    \"Trade Details\": {\n"
                "        \"Type\":          \"%s\",\n"
                "        \"Order ID\":      \"%s\",\n"
                "        \"Trader ID\":     \"%s\",\n"
                "        \"Symbol\":        \"%s\",\n"
                "        \"Price\":         %.2f,\n"
                "        \"Quantity\":      %d\n"
                "    },\n"
                "    \"Timestamp\":     \"%s\",\n"
                "    \"status\":        \"success\"\n"
                "}",
                MSG_ORDER_ACCEPTED,
                order.is_buy ? "Buy" : "Sell",
                order.order_id,
                order.trader_id,
                order.symbol,
                order.price,
                order.quantity,
                timestamp);

            ws_server_send(client, response, strlen(response));
            LOG_INFO("Order placed and confirmed: %s", response);

            LOG_INFO("Attempting to match orders for %s", order.symbol);
            order_book_match_orders(book);

            // Send updated book snapshot
            BookSnapshot snapshot = {0};
            strncpy(snapshot.symbol, order.symbol, sizeof(snapshot.symbol) - 1);
            snapshot.max_orders = 200;
            snapshot.bid_prices = malloc(snapshot.max_orders * sizeof(double));
            snapshot.bid_quantities = malloc(snapshot.max_orders * sizeof(int));
            snapshot.ask_prices = malloc(snapshot.max_orders * sizeof(double));
            snapshot.ask_quantities = malloc(snapshot.max_orders * sizeof(int));

            if (snapshot.bid_prices && snapshot.bid_quantities && 
                snapshot.ask_prices && snapshot.ask_quantities) {
                
                snapshot.num_bids = snapshot.num_asks = 0;
                avl_inorder_traverse(book->buy_orders, collect_orders, &snapshot);
                avl_inorder_traverse(book->sell_orders, collect_orders, &snapshot);

                char* book_json = serialize_book_snapshot(&snapshot);
                if (book_json) {
                    ws_server_send(client, book_json, strlen(book_json));
                    free(book_json);
                }
            }

            free(snapshot.bid_prices);
            free(snapshot.bid_quantities);
            free(snapshot.ask_prices);
            free(snapshot.ask_quantities);
        }
    }

    pthread_rwlock_unlock(&handlers->books_lock);
    
    if (!order_placed) {
        return send_error_response(client, "Failed to place order", response);
    }

    return 0;
}

int handle_cancel_order(ServerHandlers* handlers, WSClient* client, const cJSON* root, char* response) {
    const cJSON* order_id = cJSON_GetObjectItem(root, "order_id");
    const cJSON* symbol = cJSON_GetObjectItem(root, "symbol");
    
    if (!order_id || !order_id->valuestring || !symbol || !symbol->valuestring) {
        return send_error_response(client, "Missing order_id or symbol", response);
    }

    pthread_rwlock_rdlock(&handlers->books_lock);
    OrderBook* book = NULL;
    
    // Find order book
    for (int i = 0; i < handlers->book_count; i++) {
        if (strcmp(handlers->symbols[i], symbol->valuestring) == 0) {
            book = handlers->books[i];
            break;
        }
    }
    
    if (!book) {
        pthread_rwlock_unlock(&handlers->books_lock);
        return send_error_response(client, "Order book not found", response);
    }

    const cJSON* is_buy = cJSON_GetObjectItem(root, "is_buy");
    if (!is_buy) {
        pthread_rwlock_unlock(&handlers->books_lock);
        return send_error_response(client, "Missing is_buy flag", response);
    }

    // Cancel the order
    if (order_book_cancel_order(book, order_id->valuestring, is_buy->valueint) != 0) {
        pthread_rwlock_unlock(&handlers->books_lock);
        return send_error_response(client, "Order not found or already canceled", response);
    }

    pthread_rwlock_unlock(&handlers->books_lock);

    // Get current timestamp
    time_t now;
    time(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Send cancellation confirmation
    snprintf(response, 1024,
        "{\n"
        "    \"type\": %d,\n"
        "    \"Cancellation Details\": {\n"
        "        \"Order ID\":      \"%s\",\n"
        "        \"Symbol\":        \"%s\"\n"
        "    },\n"
        "    \"Timestamp\":     \"%s\",\n"
        "    \"status\":        \"success\"\n"
        "}",
        MSG_ORDER_CANCELED,
        order_id->valuestring,
        symbol->valuestring,
        timestamp);

    ws_server_send(client, response, strlen(response));
    LOG_INFO("Order canceled: %s", response);
    
    return 0;
}

int handle_book_request(ServerHandlers* handlers, WSClient* client, const cJSON* root, char* response) {
    const cJSON* symbol = cJSON_GetObjectItem(root, "symbol");
    if (!symbol || !symbol->valuestring) {
        return send_error_response(client, "Missing symbol", response);
    }

    pthread_rwlock_rdlock(&handlers->books_lock);
    OrderBook* book = NULL;

    // Find order book
    for (int i = 0; i < handlers->book_count; i++) {
        if (strcmp(handlers->symbols[i], symbol->valuestring) == 0) {
            book = handlers->books[i];
            break;
        }
    }

    if (!book) {
        pthread_rwlock_unlock(&handlers->books_lock);
        return send_error_response(client, "Order book not found", response);
    }

    // Create book snapshot
    BookSnapshot snapshot = {0};
    strncpy(snapshot.symbol, symbol->valuestring, sizeof(snapshot.symbol) - 1);
    snapshot.max_orders = 200;
    snapshot.bid_prices = malloc(snapshot.max_orders * sizeof(double));
    snapshot.bid_quantities = malloc(snapshot.max_orders * sizeof(int));
    snapshot.ask_prices = malloc(snapshot.max_orders * sizeof(double));
    snapshot.ask_quantities = malloc(snapshot.max_orders * sizeof(int));

    if (!snapshot.bid_prices || !snapshot.bid_quantities || 
        !snapshot.ask_prices || !snapshot.ask_quantities) {
        free(snapshot.bid_prices);
        free(snapshot.bid_quantities);
        free(snapshot.ask_prices);
        free(snapshot.ask_quantities);
        pthread_rwlock_unlock(&handlers->books_lock);
        return send_error_response(client, "Memory allocation failed", response);
    }

    snapshot.num_bids = snapshot.num_asks = 0;
    avl_inorder_traverse(book->buy_orders, collect_orders, &snapshot);
    avl_inorder_traverse(book->sell_orders, collect_orders, &snapshot);

    pthread_rwlock_unlock(&handlers->books_lock);

    // Serialize and send snapshot
    char* book_json = serialize_book_snapshot(&snapshot);
    if (!book_json) {
        free(snapshot.bid_prices);
        free(snapshot.bid_quantities);
        free(snapshot.ask_prices);
        free(snapshot.ask_quantities);
        return send_error_response(client, "Failed to serialize book snapshot", response);
    }

    ws_server_send(client, book_json, strlen(book_json));
    
    free(book_json);
    free(snapshot.bid_prices);
    free(snapshot.bid_quantities);
    free(snapshot.ask_prices);
    free(snapshot.ask_quantities);
    
    return 0;
}

// Worker Thread
static void* worker_thread(void* arg) {
    ServerHandlers* handlers = (ServerHandlers*)arg;
    char response[1024];
    WSClient* client;

    while (handlers->running) {
        char* message = dequeue_message(handlers, &client);
        if (!message) continue;

        int msg_type;
        if (!parse_base_message(message, &msg_type)) {
            send_error_response(client, "Invalid message format", response);
            free(message);
            continue;
        }

        cJSON* root = cJSON_Parse(message);
        if (!root) {
            send_error_response(client, "Invalid JSON", response);
            free(message);
            continue;
        }

        // Find and execute appropriate handler
        bool handled = false;
        for (size_t i = 0; i < sizeof(message_handlers)/sizeof(message_handlers[0]); i++) {
            if (message_handlers[i].msg_type == msg_type) {
                message_handlers[i].handler(handlers, client, root, response);
                handled = true;
                break;
            }
        }

        if (!handled) {
            LOG_WARN("Unhandled message type: %d", msg_type);
            send_error_response(client, "Unsupported message type", response);
        }

        cJSON_Delete(root);
        free(message);
    }
    return NULL;
}

ServerHandlers* server_handlers_create(const HandlerConfig* config) {
    ServerHandlers* handlers = calloc(1, sizeof(ServerHandlers));
    if (!handlers) return NULL;

    handlers->thread_count = config->thread_pool_size;
    handlers->queue_size = config->message_queue_size;
    handlers->running = false;
    handlers->queue_head = handlers->queue_tail = 0;

    handlers->worker_threads = calloc(config->thread_pool_size, sizeof(pthread_t));
    handlers->message_queue = calloc(config->message_queue_size, sizeof(char*));
    handlers->client_queue = calloc(config->message_queue_size, sizeof(WSClient*));

    if (!handlers->worker_threads || !handlers->message_queue || !handlers->client_queue) {
        free(handlers->worker_threads);
        free(handlers->message_queue);
        free(handlers->client_queue);
        free(handlers);
        return NULL;
    }

    pthread_mutex_init(&handlers->queue_lock, NULL);
    pthread_cond_init(&handlers->queue_cond, NULL);
    pthread_rwlock_init(&handlers->books_lock, NULL);

    return handlers;
}

void server_handlers_destroy(ServerHandlers* handlers) {
    if (!handlers) return;
    
    if (handlers->running) {
        server_handlers_stop_workers(handlers);
    }
    
    pthread_mutex_destroy(&handlers->queue_lock);
    pthread_cond_destroy(&handlers->queue_cond);
    pthread_rwlock_destroy(&handlers->books_lock);
    
    for (int i = 0; i < handlers->book_count; i++) {
        order_book_destroy(handlers->books[i]);
    }
    
    for (int i = 0; i < handlers->queue_size; i++) {
        free(handlers->message_queue[i]);
    }
    
    free(handlers->message_queue);
    free(handlers->worker_threads);
    free(handlers);
}

int server_handlers_start_workers(ServerHandlers* handlers) {
    if (!handlers || handlers->running) return -1;
    
    handlers->running = true;
    for (int i = 0; i < handlers->thread_count; i++) {
        if (pthread_create(&handlers->worker_threads[i], NULL, worker_thread, handlers) != 0) {
            handlers->running = false;
            return -1;
        }
    }
    
    return 0;
}

int server_handlers_stop_workers(ServerHandlers* handlers) {
    if (!handlers || !handlers->running) return -1;
    
    handlers->running = false;
    pthread_cond_broadcast(&handlers->queue_cond);
    
    for (int i = 0; i < handlers->thread_count; i++) {
        pthread_join(handlers->worker_threads[i], NULL);
    }
    
    return 0;
}

int server_handlers_process_message(ServerHandlers* handlers, WSClient* client, 
                                  const char* message, size_t len) {
    if (!handlers || !message) return -1;
    
    LOG_DEBUG("Processing message: %.*s", (int)len, message);

    pthread_mutex_lock(&handlers->queue_lock);
    
    if ((handlers->queue_tail + 1) % handlers->queue_size == handlers->queue_head) {
        pthread_mutex_unlock(&handlers->queue_lock);
        return -1; // Queue full
    }
    
    char* msg_copy = strdup(message);
    if (!msg_copy) {
        pthread_mutex_unlock(&handlers->queue_lock);
        return -1;
    }
    
    handlers->message_queue[handlers->queue_tail] = msg_copy;
    handlers->client_queue[handlers->queue_tail] = client;  // Store client reference
    handlers->queue_tail = (handlers->queue_tail + 1) % handlers->queue_size;
    
    pthread_cond_signal(&handlers->queue_cond);
    pthread_mutex_unlock(&handlers->queue_lock);
    
    return 0;
}

OrderBook* server_handlers_get_order_book(ServerHandlers* handlers, const char* symbol) {
    if (!handlers || !symbol) return NULL;
    
    OrderBook* book = NULL;
    pthread_rwlock_rdlock(&handlers->books_lock);
    
    for (int i = 0; i < handlers->book_count; i++) {
        if (strcmp(handlers->symbols[i], symbol) == 0) {
            book = handlers->books[i];
            break;
        }
    }
    
    pthread_rwlock_unlock(&handlers->books_lock);
    return book;
}

int server_handlers_add_order_book(ServerHandlers* handlers, const char* symbol) {
    if (!handlers || !symbol || handlers->book_count >= MAX_SYMBOLS) return -1;
    
    pthread_rwlock_wrlock(&handlers->books_lock);
    
    // Check if symbol already exists
    for (int i = 0; i < handlers->book_count; i++) {
        if (strcmp(handlers->symbols[i], symbol) == 0) {
            pthread_rwlock_unlock(&handlers->books_lock);
            return -1;
        }
    }
    
    OrderBook* book = order_book_create();
    if (!book) {
        pthread_rwlock_unlock(&handlers->books_lock);
        return -1;
    }
    
    strncpy(handlers->symbols[handlers->book_count], symbol, 15);
    handlers->symbols[handlers->book_count][15] = '\0';
    handlers->books[handlers->book_count] = book;
    handlers->book_count++;
    
    pthread_rwlock_unlock(&handlers->books_lock);
    return 0;
}
