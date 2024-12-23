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

    LOG_INFO("Trade executed: %s - Qty: %u, Price: %.2f, Buy Order: %lu, Sell Order: %lu",
             trade.symbol, trade.quantity, trade.price, 
             trade.buy_order_id, trade.sell_order_id);

    if (book->trade_callback) {
        book->trade_callback(&trade, book->callback_user_data);
    }
}

// Attempt to match an incoming order with existing orders
static bool try_match_order(OrderBook* book, Order* incoming_order) {
    if (!book || !incoming_order) return false;

    bool matched = false;
    uint32_t remaining_quantity = incoming_order->quantity;
    
    while (remaining_quantity > 0) {
        // Get best counter price
        PriceNode* counter_node = incoming_order->is_buy ? 
            book->sell_tree : book->buy_tree;
            
        // For buy orders, traverse to lowest sell price
        if (incoming_order->is_buy) {
            while (counter_node && counter_node->left) {
                counter_node = counter_node->left;
            }
        } 
        // For sell orders, traverse to highest buy price
        else {
            while (counter_node && counter_node->right) {
                counter_node = counter_node->right;
            }
        }

        if (!counter_node) break;  // No matching orders

        // Check if prices cross
        if ((incoming_order->is_buy && incoming_order->price < counter_node->price) ||
            (!incoming_order->is_buy && incoming_order->price > counter_node->price)) {
            break;  // No price match
        }

        // Process matches at this price level
        OrderNode* current = counter_node->orders;
        while (current && remaining_quantity > 0) {
            uint32_t trade_quantity = 
                (remaining_quantity < current->order.quantity) ? 
                remaining_quantity : current->order.quantity;

            // Execute trade at resting order's price
            process_trade(book, 
                        incoming_order->is_buy ? incoming_order : &current->order,
                        incoming_order->is_buy ? &current->order : incoming_order,
                        trade_quantity, counter_node->price);

            remaining_quantity -= trade_quantity;
            current->order.quantity -= trade_quantity;

            if (current->order.quantity == 0) {
                // Remove fully filled order
                counter_node->orders = current->next;
                free(current);
                counter_node->order_count--;
                book->total_orders--;
                current = counter_node->orders;
            } else {
                current = current->next;
            }

            matched = true;
        }

        if (counter_node->order_count == 0) {
            // Remove empty price level (TODO: Implement proper removal)
            if (incoming_order->is_buy) {
                book->sell_tree = NULL;  // Simplified - should properly remove node
            } else {
                book->buy_tree = NULL;  // Simplified - should properly remove node
            }
        }
    }

    // Update incoming order quantity
    incoming_order->quantity = remaining_quantity;
    return matched;
}

// Add a new price level to the book
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
    if (!book || !order) {
        LOG_ERROR("Invalid book or order pointer");
        return false;
    }

    // Validate order
    if (!order_validate(order)) {
        LOG_ERROR("Order failed validation");
        return false;
    }

    // Validate symbol match
    if (strcmp(book->symbol, order->symbol) != 0) {
        LOG_ERROR("Symbol mismatch: book '%s', order '%s'", 
                  book->symbol, order->symbol);
        return false;
    }

    // Create a modifiable copy of the order
    Order order_copy = *order;

    // Try to match first
    bool matched = try_match_order(book, &order_copy);

    // If there's remaining quantity, add to book
    if (order_copy.quantity > 0) {
        OrderNode* order_node = malloc(sizeof(OrderNode));
        if (!order_node) {
            LOG_ERROR("Failed to allocate memory for order node");
            return matched;  // Return true if we at least executed some trades
        }

        // Copy the potentially modified order
        memcpy(&order_node->order, &order_copy, sizeof(Order));
        order_node->next = NULL;

        PriceNode** tree = order->is_buy ? &book->buy_tree : &book->sell_tree;
        *tree = insert_price_node(*tree, order_copy.price, order_node);

        if (!*tree) {
            LOG_ERROR("Failed to insert price node");
            free(order_node);
            return matched;
        }

        book->total_orders++;
        LOG_DEBUG("Added order to book: id=%lu, price=%.2f, quantity=%u, is_buy=%d",
                 order_node->order.id, order_node->order.price, 
                 order_node->order.quantity, order_node->order.is_buy);
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
