#include "trading/protocol/messages.h"
#include "trading/engine/order_book.h"
#include "trading/engine/matcher.h"
#include "utils/logging.h"
#include <cJSON/cJSON.h>
#include <stdlib.h>
#include <string.h>

// Internal structures matching order_book.c
struct OrderNode {
    Order order;
    struct OrderNode* next;
};

struct PriceNode {
    double price;
    struct OrderNode* orders;
    struct PriceNode* left;
    struct PriceNode* right;
    int height;
    size_t order_count;
};

static void add_orders_to_array(struct PriceNode* node, cJSON* array) {
    if (!node || !array) {
        return;
    }

    // Add orders at this price level
    struct OrderNode* curr = node->orders;
    while (curr) {
        cJSON* order = cJSON_CreateObject();
        if (order) {
            cJSON_AddNumberToObject(order, "price", node->price);
            cJSON_AddNumberToObject(order, "quantity", curr->order.quantity);
            cJSON_AddNumberToObject(order, "id", (double)curr->order.id);
            cJSON_AddItemToArray(array, order);
        }
        curr = curr->next;
    }

    // Process children
    if (node->left) {
        add_orders_to_array(node->left, array);
    }
    if (node->right) {
        add_orders_to_array(node->right, array);
    }
}

char* trade_notification_serialize(const Trade* trade) {
    if (!trade) {
        LOG_ERROR("Cannot serialize NULL trade");
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "trade");
    cJSON_AddNumberToObject(root, "trade_id", (double)trade->id);
    cJSON_AddStringToObject(root, "symbol", trade->symbol);
    cJSON_AddNumberToObject(root, "price", trade->price);
    cJSON_AddNumberToObject(root, "quantity", trade->quantity);
    cJSON_AddNumberToObject(root, "buy_order_id", (double)trade->buy_order_id);
    cJSON_AddNumberToObject(root, "sell_order_id", (double)trade->sell_order_id);
    cJSON_AddNumberToObject(root, "timestamp", (double)trade->timestamp);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}

char* book_query_serialize(const BookQueryConfig* config) {
    LOG_DEBUG("Starting book query serialization");

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "book_response");
    cJSON* books = cJSON_CreateArray();
    if (!books) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "books", books);

    size_t max_books = order_handler_get_active_book_count();
    LOG_DEBUG("Total active order books: %zu", max_books);

    if (max_books == 0) {
        char* json_str = cJSON_Print(root);
        cJSON_Delete(root);
        return json_str;
    }

    OrderBook** active_books = calloc(max_books, sizeof(OrderBook*));
    if (!active_books) {
        cJSON_Delete(root);
        return NULL;
    }

    size_t actual_books = order_handler_get_all_books(active_books, max_books);
    LOG_DEBUG("Processing %zu active books", actual_books);

    for (size_t i = 0; i < actual_books; i++) {
        OrderBook* book = active_books[i];
        if (!book) continue;

        const char* symbol = order_book_get_symbol(book);
        if (!symbol) continue;

        cJSON* book_obj = cJSON_CreateObject();
        if (!book_obj) continue;

        cJSON_AddStringToObject(book_obj, "symbol", symbol);
        
        // Add bids
        cJSON* bids = cJSON_CreateArray();
        if (bids) {
            if (book->buy_tree) {
                add_orders_to_array(book->buy_tree, bids);
            }
            cJSON_AddItemToObject(book_obj, "bids", bids);
        }

        // Add asks
        cJSON* asks = cJSON_CreateArray();
        if (asks) {
            if (book->sell_tree) {
                add_orders_to_array(book->sell_tree, asks);
            }
            cJSON_AddItemToObject(book_obj, "asks", asks);
        }

        cJSON_AddItemToArray(books, book_obj);
    }

    free(active_books);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);
    return json_str;
}

char* order_response_serialize(uint64_t order_id, bool success, const char* message) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "order_response");
    cJSON_AddBoolToObject(root, "success", success);
    cJSON_AddNumberToObject(root, "order_id", (double)order_id);
    if (message) {
        cJSON_AddStringToObject(root, "message", message);
    }

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}

char* cancel_response_serialize(CancelResult result, uint64_t order_id) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "cancel_response");
    cJSON_AddNumberToObject(root, "order_id", (double)order_id);

    const char* status;
    bool success;

    switch (result) {
        case CANCEL_SUCCESS:
            status = "Order cancelled successfully";
            success = true;
            break;
        case CANCEL_ORDER_NOT_FOUND:
            status = "Order not found";
            success = false;
            break;
        case CANCEL_INVALID_BOOK:
            status = "Invalid order book";
            success = false;
            break;
        case CANCEL_ALREADY_FILLED:
            status = "Order already filled";
            success = false;
            break;
        default:
            status = "Unknown error";
            success = false;
    }

    cJSON_AddBoolToObject(root, "success", success);
    cJSON_AddStringToObject(root, "message", status);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}

