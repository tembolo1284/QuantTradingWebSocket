#include "trading/engine/order_book.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

struct PriceNode {
    double price;
    OrderNode* orders;
    struct PriceNode* left;
    struct PriceNode* right;
    int height;
    size_t order_count;
};

struct OrderNode {
    Order order;
    uint64_t timestamp;
    struct OrderNode* next;
};

// AVL tree helper functions
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

// Tree rotations
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

    // Left heavy
    if (balance > 1) {
        if (get_balance(node->left) < 0) {
            node->left = rotate_left(node->left);
        }
        return rotate_right(node);
    }

    // Right heavy
    if (balance < -1) {
        if (get_balance(node->right) > 0) {
            node->right = rotate_right(node->right);
        }
        return rotate_left(node);
    }

    return node;
}

// Create trade record and notify via callback
static void process_trade(OrderBook* book, const Order* buy_order, 
                        const Order* sell_order, uint32_t quantity, 
                        double execution_price) {
    if (!book || !buy_order || !sell_order) return;

    Trade trade = {
        .buy_order_id = buy_order->id,
        .sell_order_id = sell_order->id,
        .price = execution_price,
        .quantity = quantity,
        .timestamp = get_timestamp()
    };
    strncpy(trade.symbol, book->symbol, sizeof(trade.symbol) - 1);
    trade.symbol[sizeof(trade.symbol) - 1] = '\0';
    
    LOG_INFO("TRADE EXECUTED: Symbol=%s, Quantity=%u, Price=%.2f", 
             trade.symbol, trade.quantity, trade.price);
    LOG_INFO("BUY ORDER: ID=%lu, Trader=%s", 
             buy_order->id, "TODO: Add trader identifier");
    LOG_INFO("SELL ORDER: ID=%lu, Trader=%s", 
             sell_order->id, "TODO: Add trader identifier");

    if (book->trade_callback) {
        book->trade_callback(&trade, book->callback_user_data);
    }
}

static PriceNode* find_leftmost_node(PriceNode* node) {
    if (!node) return NULL;
    while (node->left) {
        node = node->left;
    }
    return node;
}

static PriceNode* find_rightmost_node(PriceNode* node) {
    if (!node) return NULL;
    while (node->right) {
        node = node->right;
    }
    return node;
}

static PriceNode* remove_price_node(PriceNode* root, double price) {
    if (!root) return NULL;
    
    if (price < root->price)
        root->left = remove_price_node(root->left, price);
    else if (price > root->price)
        root->right = remove_price_node(root->right, price);
    else {
        if (!root->left) {
            PriceNode* temp = root->right;
            free(root);
            return temp;
        } else if (!root->right) {
            PriceNode* temp = root->left;
            free(root);
            return temp;
        }
        
        PriceNode* temp = find_leftmost_node(root->right);
        root->price = temp->price;
        root->orders = temp->orders;
        root->order_count = temp->order_count;
        root->right = remove_price_node(root->right, temp->price);
    }
    
    return balance_node(root);
}

static bool try_match_order(OrderBook* book, Order* incoming_order) {
    if (!book || !incoming_order) return false;
    LOG_DEBUG("Attempting to match order: symbol=%s, price=%.2f, quantity=%u, is_buy=%d",
              book->symbol, incoming_order->price, incoming_order->quantity, incoming_order->is_buy);

    bool matched = false;
    uint32_t remaining_quantity = incoming_order->quantity;
    PriceNode** counter_tree = incoming_order->is_buy ? &book->sell_tree : &book->buy_tree;

    while (remaining_quantity > 0 && *counter_tree) {
        PriceNode* current_level = incoming_order->is_buy ?
            find_leftmost_node(*counter_tree) : find_rightmost_node(*counter_tree);
        if (!current_level) break;

        LOG_DEBUG("Checking price level: price=%.2f, order_count=%zu",
                  current_level->price, current_level->order_count);

        bool prices_cross = (incoming_order->is_buy && incoming_order->price >= current_level->price) ||
                            (!incoming_order->is_buy && incoming_order->price <= current_level->price);
        if (!prices_cross) break;

        OrderNode* current = current_level->orders;
        OrderNode* prev = NULL;

        while (current && remaining_quantity > 0) {
            uint32_t trade_quantity = (remaining_quantity < current->order.quantity) ?
                remaining_quantity : current->order.quantity;

            process_trade(book, incoming_order->is_buy ? incoming_order : &current->order,
                          incoming_order->is_buy ? &current->order : incoming_order,
                          trade_quantity, current_level->price);

            remaining_quantity -= trade_quantity;
            current->order.quantity -= trade_quantity;
            matched = true;

            LOG_DEBUG("Trade executed: trade_qty=%u, remaining_incoming_qty=%u, current_order_qty=%u",
                      trade_quantity, remaining_quantity, current->order.quantity);

            if (current->order.quantity == 0) {
                OrderNode* to_free = current;
                current = current->next;
                if (prev) prev->next = current;
                else current_level->orders = current;
                free(to_free);
                current_level->order_count--;
                book->total_orders--;
            } else {
                prev = current;
                current = current->next;
            }
        }

        if (current_level->order_count == 0) {
            *counter_tree = remove_price_node(*counter_tree, current_level->price);
        }
    }

    incoming_order->quantity = remaining_quantity;
    LOG_DEBUG("Matching complete: original_qty=%u, remaining_qty=%u, matched=%d",
              incoming_order->quantity, remaining_quantity, matched);
    return matched;
}

static PriceNode* insert_price_node(PriceNode* node, double price, OrderNode* new_order) {
    if (!node) {
        PriceNode* new_node = malloc(sizeof(PriceNode));
        if (!new_node) return NULL;
        new_node->price = price;
        new_node->orders = new_order;
        new_node->left = new_node->right = NULL;
        new_node->height = 1;
        new_node->order_count = 1;
        return new_node;
    }

    if (price < node->price) {
        node->left = insert_price_node(node->left, price, new_order);
    } else if (price > node->price) {
        node->right = insert_price_node(node->right, price, new_order);
    } else {
        // Insert new order in time priority
        OrderNode** current = &node->orders;
        while (*current && (*current)->timestamp <= new_order->timestamp) {
            current = &(*current)->next;
        }
        new_order->next = *current;
        *current = new_order;
        node->order_count++;
    }

    return balance_node(node);
}

/*
static void collect_orders_in_order(PriceNode* node, OrderNode** order_list) {
    if (!node) return;

    // First, traverse the left subtree (lower prices)
    collect_orders_in_order(node->left, order_list);

    // Add orders at this price level in their original order
    OrderNode* current = node->orders;
    OrderNode* last = NULL;
    while (current) {
        OrderNode* next = current->next;
        
        // Prepend to maintain original order
        current->next = *order_list;
        *order_list = current;
        
        current = next;
    }

    // Then traverse the right subtree (higher prices)
    collect_orders_in_order(node->right, order_list);
}

*/

// Public functions
OrderBook* order_book_create(const char* symbol) {
    if (!symbol || *symbol == '\0') return NULL;

    OrderBook* book = malloc(sizeof(OrderBook));
    if (!book) return NULL;

    book->buy_tree = NULL;
    book->sell_tree = NULL;
    book->total_orders = 0;
    book->trade_callback = NULL;
    book->callback_user_data = NULL;
    
    strncpy(book->symbol, symbol, sizeof(book->symbol) - 1);
    book->symbol[sizeof(book->symbol) - 1] = '\0';

    LOG_INFO("Created order book for symbol: %s", book->symbol);
    return book;
}

bool order_book_add(OrderBook* book, const Order* order) {
    if (!book || !order || !order_validate(order) || strcmp(book->symbol, order->symbol) != 0) {
        LOG_ERROR("Invalid order or book");
        return false;
    }

    Order order_copy = *order;
    bool matched = try_match_order(book, &order_copy);

    if (order_copy.quantity > 0) {
        OrderNode* order_node = malloc(sizeof(OrderNode));
        if (!order_node) {
            LOG_ERROR("Failed to allocate memory for order node");
            return matched;
        }

        order_node->order = order_copy;
        order_node->timestamp = get_timestamp();
        order_node->next = NULL;

        PriceNode** tree = order_copy.is_buy ? &book->buy_tree : &book->sell_tree;
        *tree = insert_price_node(*tree, order_copy.price, order_node);

        if (!*tree) {
            LOG_ERROR("Failed to insert price node");
            free(order_node);
            return matched;
        }

        book->total_orders++;
    }

    return true;
}

CancelResult order_book_cancel(OrderBook* book, uint64_t order_id) {
    if (!book) {
        LOG_ERROR("Invalid order book pointer");
        return CANCEL_INVALID_BOOK;
    }

    // Helper function to find and remove order from a price tree
    bool found = false;
    
    // Try buy tree first
    PriceNode* node = book->buy_tree;
    while (node && !found) {
        OrderNode* prev = NULL;
        OrderNode* current = node->orders;
        
        while (current) {
            if (current->order.id == order_id) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    node->orders = current->next;
                }
                node->order_count--;
                book->total_orders--;
                free(current);
                found = true;
                break;
            }
            prev = current;
            current = current->next;
        }
        
        if (!found) {
            node = node->right;  // Check next price level
        }
    }

    // If not found in buy tree, try sell tree
    if (!found) {
        node = book->sell_tree;
        while (node && !found) {
            OrderNode* prev = NULL;
            OrderNode* current = node->orders;
            
            while (current) {
                if (current->order.id == order_id) {
                    if (prev) {
                        prev->next = current->next;
                    } else {
                        node->orders = current->next;
                    }
                    node->order_count--;
                    book->total_orders--;
                    free(current);
                    found = true;
                    break;
                }
                prev = current;
                current = current->next;
            }
            
            if (!found) {
                node = node->right;  // Check next price level
            }
        }
    }

    if (found) {
        LOG_INFO("Order cancelled successfully: id=%lu", order_id);
        return CANCEL_SUCCESS;
    }

    LOG_WARN("Order not found for cancellation: id=%lu", order_id);
    return CANCEL_ORDER_NOT_FOUND;
}

double order_book_get_best_bid(const OrderBook* book) {
    if (!book || !book->buy_tree) return 0.0;

    return find_rightmost_node(book->buy_tree)->price ;
}

double order_book_get_best_ask(const OrderBook* book) {
    if (!book || !book->sell_tree) return 0.0;

    return find_leftmost_node(book->sell_tree)->price;
}

void order_book_set_trade_callback(OrderBook* book, 
                                 TradeCallback callback, 
                                 void* user_data) {
    if (book) {
        book->trade_callback = callback;
        book->callback_user_data = user_data;
    }
}

const char* order_book_get_symbol(const OrderBook* book) {
    return book ? book->symbol : NULL;
}

size_t order_book_get_order_count(const OrderBook* book) {
    return book ? book->total_orders : 0;
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
    if (!book) return;

    // Free both buy and sell trees
    free_price_tree(book->buy_tree);
    free_price_tree(book->sell_tree);
    
    // Free the book itself
    free(book);
    LOG_DEBUG("Order book destroyed");
}
