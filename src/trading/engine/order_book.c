#include "trading/engine/order_book.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

// AVL tree node structure
struct PriceNode {
    double price;
    OrderNode* orders;      // Linked list of orders at this price
    struct PriceNode* left;
    struct PriceNode* right;
    int height;
    size_t order_count;    // Number of orders at this price level
};

// Order node in the linked list
struct OrderNode {
    Order order;
    struct OrderNode* next;
};

static int get_height(PriceNode* node) {
    return node ? node->height : 0;
}

static int get_balance(PriceNode* node) {
    return node ? get_height(node->left) - get_height(node->right) : 0;
}

static void update_height(PriceNode* node) {
    if (node) {
        int left_height = get_height(node->left);
        int right_height = get_height(node->right);
        node->height = (left_height > right_height ? left_height : right_height) + 1;
    }
}

static PriceNode* rotate_right(PriceNode* y) {
    PriceNode* x = y->left;
    PriceNode* T2 = x->right;

    x->right = y;
    y->left = T2;

    update_height(y);
    update_height(x);

    return x;
}

static PriceNode* rotate_left(PriceNode* x) {
    PriceNode* y = x->right;
    PriceNode* T2 = y->left;

    y->left = x;
    x->right = T2;

    update_height(x);
    update_height(y);

    return y;
}

static PriceNode* balance_node(PriceNode* node) {
    if (!node) return NULL;

    update_height(node);
    int balance = get_balance(node);

    // Left Heavy
    if (balance > 1) {
        if (get_balance(node->left) < 0) {
            node->left = rotate_left(node->left);
        }
        return rotate_right(node);
    }

    // Right Heavy
    if (balance < -1) {
        if (get_balance(node->right) > 0) {
            node->right = rotate_right(node->right);
        }
        return rotate_left(node);
    }

    return node;
}

__attribute__((unused))
static PriceNode* find_price_node(PriceNode* root, double price) {
    while (root) {
        if (price == root->price) {
            return root;
        }
        root = (price < root->price) ? root->left : root->right;
    }
    return NULL;
}

static PriceNode* insert_price_node(PriceNode* node, double price, OrderNode* order) {
    if (!node) {
        PriceNode* new_node = malloc(sizeof(PriceNode));
        if (!new_node) return NULL;
        
        new_node->price = price;
        new_node->orders = order;
        new_node->left = new_node->right = NULL;
        new_node->height = 1;
        new_node->order_count = 1;
        return new_node;
    }

    if (price < node->price) {
        node->left = insert_price_node(node->left, price, order);
    } else if (price > node->price) {
        node->right = insert_price_node(node->right, price, order);
    } else {
        // Price level exists, prepend order to list
        order->next = node->orders;
        node->orders = order;
        node->order_count++;
        return node;
    }

    return balance_node(node);
}

OrderBook* order_book_create(const char* symbol) {
    if (!symbol || *symbol == '\0'){
        symbol = "AAPL";
    }

    OrderBook* book = malloc(sizeof(OrderBook));
    if (!book) return NULL;

    book->buy_tree = NULL;
    book->sell_tree = NULL;
    book->total_orders = 0;
    strncpy(book->symbol, symbol, sizeof(book->symbol) - 1);
    book->symbol[sizeof(book->symbol) - 1] = '\0';

    return book;
}

bool order_book_add(OrderBook* book, const Order* order) {
    // Extensive logging for debugging
    LOG_DEBUG("Attempting to add order: "
              "id=%lu, price=%.2f, quantity=%u, is_buy=%d, symbol=%s", 
              order->id, order->price, order->quantity, 
              order->is_buy, order->symbol);

    // Validate input parameters
    if (!book) {
        LOG_ERROR("Null order book");
        return false;
    }

    if (!order) {
        LOG_ERROR("Null order");
        return false;
    }

    // Validate order
    if (!order_validate(order)) {
        LOG_ERROR("Order failed validation: "
                  "price=%.2f, quantity=%u, symbol=%s, is_buy=%d", 
                  order->price, order->quantity, order->symbol, order->is_buy);
        return false;
    }

    // Validate symbol match
    if (strcmp(book->symbol, order->symbol) != 0) {
        LOG_ERROR("Symbol mismatch: book symbol '%s', order symbol '%s'", 
                  book->symbol, order->symbol);
        return false;
    }

    // Create new order node
    OrderNode* order_node = malloc(sizeof(OrderNode));
    if (!order_node) {
        LOG_ERROR("Failed to allocate memory for order node");
        return false;
    }

    // Deep copy the order
    memcpy(&order_node->order, order, sizeof(Order));
    order_node->next = NULL;

    // Insert into appropriate tree
    PriceNode** tree = order->is_buy ? &book->buy_tree : &book->sell_tree;
    
    // Log tree state before insertion
    LOG_DEBUG("Inserting %s order at price %.2f", 
              order->is_buy ? "BUY" : "SELL", order->price);

    // Attempt to insert
    *tree = insert_price_node(*tree, order->price, order_node);

    // Check if insertion was successful
    if (!*tree) {
        LOG_ERROR("Failed to insert price node");
        free(order_node);
        return false;
    }

    book->total_orders++;
    
    LOG_DEBUG("Order added successfully. Total orders: %lu", book->total_orders);
    return true;
}

static OrderNode* remove_order_from_list(OrderNode* head, uint64_t order_id, bool* found) {
    if (!head) return NULL;

    *found = false;
    if (head->order.id == order_id) {
        OrderNode* next = head->next;
        free(head);
        *found = true;
        return next;
    }

    OrderNode* current = head;
    while (current->next) {
        if (current->next->order.id == order_id) {
            OrderNode* to_remove = current->next;
            current->next = to_remove->next;
            free(to_remove);
            *found = true;
            break;
        }
        current = current->next;
    }

    return head;
}

bool order_book_cancel(OrderBook* book, uint64_t order_id) {
    if (!book || !order_id) return false;

    bool found = false;
    PriceNode* node;

    // Try buy tree first
    node = book->buy_tree;
    while (node && !found) {
        node->orders = remove_order_from_list(node->orders, order_id, &found);
        if (found) {
            node->order_count--;
            book->total_orders--;
            return true;
        }
        node = node->right;  // Check next price level
    }

    // Try sell tree if not found
    node = book->sell_tree;
    while (node && !found) {
        node->orders = remove_order_from_list(node->orders, order_id, &found);
        if (found) {
            node->order_count--;
            book->total_orders--;
            return true;
        }
        node = node->right;
    }

    return false;
}

double order_book_get_best_bid(const OrderBook* book) {
    if (!book || !book->buy_tree) return 0.0;

    // Find rightmost node in buy tree (highest price)
    PriceNode* node = book->buy_tree;
    while (node->right) {
        node = node->right;
    }
    return node->price;
}

double order_book_get_best_ask(const OrderBook* book) {
    if (!book || !book->sell_tree) return 0.0;

    // Find leftmost node in sell tree (lowest price)
    PriceNode* node = book->sell_tree;
    while (node->left) {
        node = node->left;
    }
    return node->price;
}

static void free_order_list(OrderNode* head) {
    while (head) {
        OrderNode* next = head->next;
        free(head);
        head = next;
    }
}

static void free_price_tree(PriceNode* root) {
    if (root) {
        free_price_tree(root->left);
        free_price_tree(root->right);
        free_order_list(root->orders);
        free(root);
    }
}

void order_book_destroy(OrderBook* book) {
    if (book) {
        free_price_tree(book->buy_tree);
        free_price_tree(book->sell_tree);
        free(book);
    }
}
