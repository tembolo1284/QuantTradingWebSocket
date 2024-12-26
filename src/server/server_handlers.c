#include "server/server_handlers.h"
#include "protocol/json_protocol.h"
#include "trading_engine/order.h"
#include "utils/logging.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

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

static void collect_orders(Order* order, void* user_data) {
    BookSnapshot* snap = (BookSnapshot*)user_data;
    if (order->is_canceled || order->remaining_quantity <= 0) {
        return;  // Skip canceled or fully filled orders
    }

    if (order_is_buy_order(order)) {
        if ((size_t)snap->num_bids < snap->max_orders) {  // Fix signedness comparison
            snap->bid_prices[snap->num_bids] = order_get_price(order);
            snap->bid_quantities[snap->num_bids] = order_get_remaining_quantity(order);
            snap->num_bids++;
        }
    } else {
        if ((size_t)snap->num_asks < snap->max_orders) {  // Fix signedness comparison
            snap->ask_prices[snap->num_asks] = order_get_price(order);
            snap->ask_quantities[snap->num_asks] = order_get_remaining_quantity(order);
            snap->num_asks++;
        }
    }
}

static void* worker_thread(void* arg) {
    ServerHandlers* handlers = (ServerHandlers*)arg;
    
    while (handlers->running) {
        pthread_mutex_lock(&handlers->queue_lock);
        
        // Wait for messages if queue is empty
        while (handlers->queue_head == handlers->queue_tail && handlers->running) {
            pthread_cond_wait(&handlers->queue_cond, &handlers->queue_lock);
        }
        
        if (!handlers->running) {
            pthread_mutex_unlock(&handlers->queue_lock);
            break;
        }
        
        // Get message and client from queue
        char* message = handlers->message_queue[handlers->queue_head];
        WSClient* client = handlers->client_queue[handlers->queue_head];
        handlers->queue_head = (handlers->queue_head + 1) % handlers->queue_size;
        
        pthread_mutex_unlock(&handlers->queue_lock);
        
        // Process message
        int msg_type;
        char response[1024];  // Define response buffer here
        
        if (parse_base_message(message, &msg_type)) {
            cJSON* root = cJSON_Parse(message);
            if (!root) {
                LOG_ERROR("Failed to parse JSON message");
                snprintf(response, sizeof(response),
                        "{\"type\": %d, \"status\": \"failed\", \"reason\": \"invalid JSON\"}",
                        MSG_ERROR);
                ws_server_send(client, response, strlen(response));
                free(message);
                continue;
            }
            
            switch (msg_type) {
                case MSG_PLACE_ORDER: {
                    OrderMessage order;
                    if (parse_order_message(message, &order)) {
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
                                
                                // Send confirmation to client
                                snprintf(response, sizeof(response),
                                    "{\"type\": %d, \"order_id\": \"%s\", \"symbol\": \"%s\", "
                                    "\"price\": %.2f, \"quantity\": %d, \"side\": \"%s\", \"status\": \"success\"}",
                                    MSG_ORDER_ACCEPTED, order.order_id, order.symbol,
                                    order.price, order.quantity, order.is_buy ? "BUY" : "SELL");
                                ws_server_send(client, response, strlen(response));
                                LOG_INFO("Order placed and confirmed: %s", response);

                                // Try to match orders
                                LOG_INFO("Attempting to match orders for %s", order.symbol);
                                order_book_match_orders(book);

                                // Send updated book snapshot
                                BookSnapshot snapshot = {0};
                                // ... fill snapshot ...
                                char* book_json = serialize_book_snapshot(&snapshot);
                                if (book_json) {
                                    ws_server_send(client, book_json, strlen(book_json));
                                    free(book_json);
                                }
                            }
                        }
                        if (!order_placed) {
                            LOG_ERROR("Failed to place order for symbol %s", order.symbol);
                            snprintf(response, sizeof(response),
                                    "{\"type\": %d, \"order_id\": \"%s\", \"status\": \"failed\", \"reason\": \"failed to place order\"}",
                                    MSG_ORDER_REJECTED, order.order_id);
                            ws_server_send(client, response, strlen(response));
                        }
                        
                        pthread_rwlock_unlock(&handlers->books_lock);
                    }
                    break;
                }

                case MSG_CANCEL_ORDER: {
                    const char* order_id = cJSON_GetObjectItem(root, "order_id")->valuestring;
                    const char* trader_id = cJSON_GetObjectItem(root, "trader_id")->valuestring;
                    bool is_buy = cJSON_GetObjectItem(root, "is_buy")->valueint;
                    
                    LOG_INFO("Cancel request: OrderID=%s, TraderID=%s", order_id, trader_id);
                    
                    int result = order_book_cancel_order(handlers->books[0], order_id, is_buy);
                    if (result == 0) {
                        snprintf(response, sizeof(response),
                                "{\"type\": %d, \"order_id\": \"%s\", \"status\": \"success\"}",
                                MSG_ORDER_CANCELED, order_id);
                    } else {
                        snprintf(response, sizeof(response),
                                "{\"type\": %d, \"order_id\": \"%s\", \"status\": \"failed\", \"reason\": \"order not found\"}",
                                MSG_ORDER_REJECTED, order_id);
                    }
                    ws_server_send(client, response, strlen(response));
                    break;
                }
                
                case MSG_REQUEST_BOOK: {
                    const char* symbol = cJSON_GetObjectItem(root, "symbol")->valuestring;
                    LOG_INFO("Book request for symbol: %s", symbol);
                    
                    pthread_rwlock_rdlock(&handlers->books_lock);
                    bool book_found = false;
                    
                    for (int i = 0; i < handlers->book_count; i++) {
                        if (strcmp(handlers->symbols[i], symbol) == 0) {
                            OrderBook* book = handlers->books[i];
                            
                            // Create book snapshot
                            BookSnapshot snapshot = {0};
                            strncpy(snapshot.symbol, symbol, sizeof(snapshot.symbol) - 1);

                            // Get all orders and sort them into bids and asks
                            snapshot.max_orders = 200;  // Reasonable limit for order book depth
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
                                LOG_ERROR("Failed to allocate memory for book snapshot");
                                break;
                            }

                            snapshot.num_bids = 0;
                            snapshot.num_asks = 0;

                            // Collect all orders
                            avl_inorder_traverse(book->buy_orders, collect_orders, &snapshot);
                            avl_inorder_traverse(book->sell_orders, collect_orders, &snapshot);

                            char* book_json = serialize_book_snapshot(&snapshot);
                            if (book_json) {
                                LOG_INFO("Sending book snapshot for %s: %s", symbol, book_json);
                                ws_server_send(client, book_json, strlen(book_json));
                                free(book_json);
                                book_found = true;
                            }

                            // Cleanup
                            free(snapshot.bid_prices);
                            free(snapshot.bid_quantities);
                            free(snapshot.ask_prices);
                            free(snapshot.ask_quantities);
                            break;
                        }
                    }    
                    if (!book_found) {
                        snprintf(response, sizeof(response),
                                "{\"type\": %d, \"symbol\": \"%s\", \"status\": \"failed\", \"reason\": \"symbol not found\"}",
                                MSG_ERROR, symbol);
                        ws_server_send(client, response, strlen(response));
                    }
                    
                    pthread_rwlock_unlock(&handlers->books_lock);
                    break;
                }
            }
            
            cJSON_Delete(root);
        } else {
            LOG_ERROR("Failed to parse message type");
            snprintf(response, sizeof(response),
                    "{\"type\": %d, \"status\": \"failed\", \"reason\": \"invalid message format\"}",
                    MSG_ERROR);
            ws_server_send(client, response, strlen(response));
        }
        
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
    
    if (!handlers->worker_threads || !handlers->message_queue) {
        free(handlers->worker_threads);
        free(handlers->message_queue);
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
