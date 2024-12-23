#include "trading/protocol/messages.h"
#include "trading/engine/order_book.h"
#include "trading/engine/matcher.h"
#include "utils/logging.h"
#include <cJSON/cJSON.h>
#include <stdlib.h>
#include <string.h>

// Since order_book.c's internal structures are private, we need these definitions
// to match the ones in order_book.c
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

// Helper function to add orders to an array
static cJSON* add_orders_to_array(struct PriceNode* tree, bool ascending) {
    cJSON* orders_array = cJSON_CreateArray();
    
    // Collect and add orders in a list to maintain order
    struct OrderNode* order_list = NULL;
    struct OrderNode* tail = NULL;

    // Traverse the tree to collect orders
    struct PriceNode* current = tree;

    // Find the extreme node based on traversal direction
    if (ascending) {
        while (current && current->left) current = current->left;
    } else {
        while (current && current->right) current = current->right;
    }

    // Collect all orders preserving their original order
    while (current) {
        // Add orders at this price level
        struct OrderNode* node_order = current->orders;
        while (node_order) {
            // Create a new list node
            struct OrderNode* list_node = malloc(sizeof(struct OrderNode));
            if (list_node) {
                memcpy(&list_node->order, &node_order->order, sizeof(Order));
                list_node->next = NULL;
                
                // Add to end of list
                if (tail) {
                    tail->next = list_node;
                    tail = list_node;
                } else {
                    order_list = tail = list_node;
                }
            }
            node_order = node_order->next;
        }

        // Move to next node in tree based on traversal direction
        if (ascending) {
            // Move to right subtree or parent
            if (current->right) {
                current = current->right;
                while (current->left) current = current->left;
            } else {
                // Simulate parent traversal without parent pointer
                struct PriceNode* prev = current;
                current = NULL;
                
                // Search entire tree for next node
                struct PriceNode* root = tree;
                while (root) {
                    if (root->price > prev->price) {
                        current = root;
                        break;
                    }
                    
                    // Decide which subtree to traverse
                    if (prev->price < root->price) {
                        root = root->left;
                    } else {
                        root = root->right;
                    }
                }
            }
        } else {
            // Move to left subtree or parent
            if (current->left) {
                current = current->left;
                while (current->right) current = current->right;
            } else {
                // Simulate parent traversal without parent pointer
                struct PriceNode* prev = current;
                current = NULL;
                
                // Search entire tree for next node
                struct PriceNode* root = tree;
                while (root) {
                    if (root->price < prev->price) {
                        current = root;
                        break;
                    }
                    
                    // Decide which subtree to traverse
                    if (prev->price > root->price) {
                        root = root->right;
                    } else {
                        root = root->left;
                    }
                }
            }
        }
    }

    // Convert collected orders to JSON
    while (order_list) {
        cJSON* order_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(order_obj, "id", (double)order_list->order.id);
        cJSON_AddNumberToObject(order_obj, "price", order_list->order.price);
        cJSON_AddNumberToObject(order_obj, "quantity", order_list->order.quantity);
        cJSON_AddItemToArray(orders_array, order_obj);

        // Free the temporary list node
        struct OrderNode* temp = order_list;
        order_list = order_list->next;
        free(temp);
    }

    return orders_array;
}

char* book_query_serialize(const BookQueryConfig* config) {
    if (!config) {
        LOG_ERROR("Invalid book query configuration");
        return NULL;
    }

    // Create root JSON object
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "book_response");

    // Create symbols array
    cJSON* symbols_array = cJSON_CreateArray();
    if (!symbols_array) {
        LOG_ERROR("Failed to create symbols array");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "symbols", symbols_array);

    // Get count of active order books
    size_t max_books = order_handler_get_active_book_count();
    if (max_books > 0) {
        OrderBook** books = malloc(sizeof(OrderBook*) * max_books);
        if (!books) {
            LOG_ERROR("Failed to allocate memory for order books");
            cJSON_Delete(root);
            return NULL;
        }

        size_t actual_books = order_handler_get_all_books(books, max_books);
        LOG_DEBUG("Processing %zu active order books", actual_books);

        for (size_t i = 0; i < actual_books; i++) {
            if (!books[i]) continue;

            // Skip if we're querying a specific symbol and this isn't it
            if (config->type == BOOK_QUERY_SYMBOL && 
                strcmp(order_book_get_symbol(books[i]), config->symbol) != 0) {
                continue;
            }

            // Add this book to the response
            cJSON* symbol_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(symbol_obj, "symbol", order_book_get_symbol(books[i]));

            // Add buy orders (descending price order)
            cJSON* buy_orders = add_orders_to_array(books[i]->buy_tree, false);
            cJSON_AddItemToObject(symbol_obj, "buy_orders", buy_orders);

            // Add sell orders (ascending price order)
            cJSON* sell_orders = add_orders_to_array(books[i]->sell_tree, true);
            cJSON_AddItemToObject(symbol_obj, "sell_orders", sell_orders);

            cJSON_AddItemToArray(symbols_array, symbol_obj);
        }

        free(books);
    }

    // Convert to string
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
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
