#include "trading/protocol/messages.h"
#include "trading/engine/order_book.h"
#include "trading/engine/matcher.h"
#include "utils/logging.h"
#include <cJSON/cJSON.h>
#include <stdlib.h>
#include <string.h>

// Structure definitions to match order_book.c's internal structures
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

// Find leftmost node in a subtree
static struct PriceNode* find_leftmost(struct PriceNode* node) {
    if (!node) return NULL;
    while (node->left) {
        node = node->left;
    }
    return node;
}

// Find rightmost node in a subtree
static struct PriceNode* find_rightmost(struct PriceNode* node) {
    if (!node) return NULL;
    while (node->right) {
        node = node->right;
    }
    return node;
}

static struct PriceNode* find_next_node_ascending(struct PriceNode* root, struct PriceNode* current) {
    if (!root || !current) return NULL;

    // If current node has a right subtree, find leftmost node in right subtree
    if (current->right) {
        return find_leftmost(current->right);
    }

    // Otherwise, find the nearest ancestor where current node is in the left subtree
    struct PriceNode* ancestor = NULL;
    struct PriceNode* search = root;
    
    while (search && search != current) {
        if (current->price < search->price) {
            ancestor = search;
            search = search->left;
        } else {
            search = search->right;
        }
    }

    return ancestor;
}

static struct PriceNode* find_next_node_descending(struct PriceNode* root, struct PriceNode* current) {
    if (!root || !current) return NULL;

    // If current node has a left subtree, find rightmost node in left subtree
    if (current->left) {
        return find_rightmost(current->left);
    }

    // Otherwise, find the nearest ancestor where current node is in the right subtree
    struct PriceNode* ancestor = NULL;
    struct PriceNode* search = root;
    
    while (search && search != current) {
        if (current->price > search->price) {
            ancestor = search;
            search = search->right;
        } else {
            search = search->left;
        }
    }

    return ancestor;
}

static cJSON* add_safe_orders_to_array(struct PriceNode* tree, bool ascending) {
    cJSON* orders_array = cJSON_CreateArray();
    if (!tree) {
        LOG_DEBUG("Empty tree provided for serialization");
        return orders_array;
    }

    struct PriceNode* current = ascending ? find_leftmost(tree) : find_rightmost(tree);

    while (current) {
        if (!current || !current->orders) { // Defensive checks
            current = ascending ?
                find_next_node_ascending(tree, current) :
                find_next_node_descending(tree, current);
            continue;
        }

        struct OrderNode* node_order = current->orders;
        while (node_order) {
            cJSON* order_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(order_obj, "id", (double)node_order->order.id);
            cJSON_AddNumberToObject(order_obj, "price", node_order->order.price);
            cJSON_AddNumberToObject(order_obj, "quantity", node_order->order.quantity);
            cJSON_AddItemToArray(orders_array, order_obj);

            node_order = node_order->next;
        }

        current = ascending ?
            find_next_node_ascending(tree, current) :
            find_next_node_descending(tree, current);
    }

    return orders_array;
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

    if (!config) {
        LOG_ERROR("Invalid book query configuration");
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON* symbols_array = cJSON_CreateArray();

    if (!root || !symbols_array) {
        LOG_ERROR("Failed to create JSON objects");
        cJSON_Delete(root);
        cJSON_Delete(symbols_array);
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "book_response");
    cJSON_AddItemToObject(root, "symbols", symbols_array);

    size_t max_books = order_handler_get_active_book_count();
    LOG_DEBUG("Total active order books: %zu", max_books);

    if (max_books == 0) {
        char* json_str = cJSON_Print(root);
        cJSON_Delete(root);
        LOG_DEBUG("Returning empty book response");
        return json_str;
    }

    OrderBook** books = calloc(max_books, sizeof(OrderBook*));
    if (!books) {
        LOG_ERROR("Memory allocation failed for order books");
        cJSON_Delete(root);
        return NULL;
    }

    size_t actual_books = order_handler_get_all_books(books, max_books);
    LOG_DEBUG("Processed active order books: %zu", actual_books);

    for (size_t i = 0; i < actual_books; i++) {
        if (!books[i]) {
            LOG_WARN("Null order book at index %zu", i);
            continue;
        }

        const char* symbol = order_book_get_symbol(books[i]);
        if (!symbol) {
            LOG_WARN("Null symbol for order book at index %zu", i);
            continue;
        }

        LOG_DEBUG("Processing book for symbol: %s", symbol);
        LOG_DEBUG("Buy tree: %p, Sell tree: %p",
                  (void*)books[i]->buy_tree,
                  (void*)books[i]->sell_tree);

        cJSON* symbol_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(symbol_obj, "symbol", symbol);

        // Safely handle buy and sell trees
        cJSON* buy_orders = books[i]->buy_tree ?
            add_safe_orders_to_array(books[i]->buy_tree, false) :
            cJSON_CreateArray();

        cJSON* sell_orders = books[i]->sell_tree ?
            add_safe_orders_to_array(books[i]->sell_tree, true) :
            cJSON_CreateArray();

        LOG_DEBUG("Buy orders count: %d, Sell orders count: %d",
                  cJSON_GetArraySize(buy_orders),
                  cJSON_GetArraySize(sell_orders));

        cJSON_AddItemToObject(symbol_obj, "buy_orders", buy_orders);
        cJSON_AddItemToObject(symbol_obj, "sell_orders", sell_orders);

        cJSON_AddItemToArray(symbols_array, symbol_obj);
    }

    free(books);
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
