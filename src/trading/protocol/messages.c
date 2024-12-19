#include "trading/protocol/messages.h"
#include "trading/engine/matcher.h"
#include "trading/engine/order_book.h"
#include "utils/logging.h"
#include <cJSON/cJSON.h>
#include <stdlib.h>
#include <string.h>

// Include the full structure definitions
#include "trading/engine/order.h"

// Use the actual structure definitions from order_book_internal implementation
struct OrderNode {
    Order order;
    struct OrderNode* next;
};

struct PriceNode {
    double price;
    struct OrderNode* orders;      // Linked list of orders at this price
    struct PriceNode* left;
    struct PriceNode* right;
    int height;
    size_t order_count;    // Number of orders at this price level
};

// Helper function to add order details to JSON
static void add_order_details_to_json(cJSON* symbol_orders, struct PriceNode* node, bool is_buy) {
    if (!node) return;

    // Recursive traversal for AVL tree
    add_order_details_to_json(symbol_orders, node->left, is_buy);

    // Create price level object
    cJSON* price_level = cJSON_CreateObject();
    cJSON_AddNumberToObject(price_level, "price", node->price);

    // Create orders array for this price level
    cJSON* orders_array = cJSON_CreateArray();

    struct OrderNode* current_order = node->orders;
    while (current_order) {
        cJSON* order_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(order_obj, "id", current_order->order.id);
        cJSON_AddNumberToObject(order_obj, "quantity", current_order->order.quantity);
        cJSON_AddBoolToObject(order_obj, "is_buy", is_buy);

        cJSON_AddItemToArray(orders_array, order_obj);
        current_order = current_order->next;
    }

    cJSON_AddItemToObject(price_level, "orders", orders_array);
    cJSON_AddItemToArray(symbol_orders, price_level);

    // Recursive traversal for right subtree
    add_order_details_to_json(symbol_orders, node->right, is_buy);
}

static void add_book_to_array(OrderBook* book, cJSON* symbols_array) {
    if (!book) return;

    cJSON* symbol_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(symbol_obj, "symbol", book->symbol);

    cJSON* buy_orders = cJSON_CreateArray();
    cJSON* sell_orders = cJSON_CreateArray();

    add_order_details_to_json(buy_orders, book->buy_tree, true);
    add_order_details_to_json(sell_orders, book->sell_tree, false);

    cJSON_AddItemToObject(symbol_obj, "buy_orders", buy_orders);
    cJSON_AddItemToObject(symbol_obj, "sell_orders", sell_orders);
    cJSON_AddItemToArray(symbols_array, symbol_obj);
}

char* book_query_serialize(const BookQueryConfig* config) {
    if (!config) {
        LOG_ERROR("Invalid book query configuration");
        return NULL;
    }

    // Root JSON object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "book_response");

    // Symbols container
    cJSON* symbols_array = cJSON_CreateArray();

    if (config->type == BOOK_QUERY_SYMBOL) {
        // Specific symbol query
        OrderBook* book = order_handler_get_book_by_symbol(config->symbol);
        if (!book) {
            // Create empty book response for requested symbol
            cJSON* symbol_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(symbol_obj, "symbol", config->symbol);
            cJSON_AddItemToObject(symbol_obj, "buy_orders", cJSON_CreateArray());
            cJSON_AddItemToObject(symbol_obj, "sell_orders", cJSON_CreateArray());
            cJSON_AddItemToArray(symbols_array, symbol_obj);
        } else {
            add_book_to_array(book, symbols_array);
        }
    } else {  // All symbols query
        size_t max_books = order_handler_get_active_book_count();
        if (max_books > 0) {
            OrderBook** books = malloc(sizeof(OrderBook*) * max_books);
            if (books) {
                size_t actual_books = order_handler_get_all_books(books, max_books);
                LOG_DEBUG("Processing %zu active order books", actual_books);

                for (size_t i = 0; i < actual_books; i++) {
                    add_book_to_array(books[i], symbols_array);
                }
                free(books);
            }
        }
        // For all-symbol query, empty array is fine if no books exist
    }

    cJSON_AddItemToObject(root, "symbols", symbols_array);

    // Convert to string
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}
