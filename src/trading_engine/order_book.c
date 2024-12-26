#include "trading_engine/order_book.h"
#include "trading_engine/order.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

OrderBook* order_book_create(void) {
    OrderBook* book = (OrderBook*)malloc(sizeof(OrderBook));
    if (!book) {
        LOG_ERROR("Failed to allocate memory for order book");
        return NULL;
    }

    book->buy_orders = avl_create(true);
    if (!book->buy_orders) {
        LOG_ERROR("Failed to create buy orders AVL tree");
        free(book);
        return NULL;
    }

    book->sell_orders = avl_create(false);
    if (!book->sell_orders) {
        LOG_ERROR("Failed to create sell orders AVL tree");
        avl_destroy(book->buy_orders);
        free(book);
        return NULL;
    }

    LOG_INFO("Created new order book");
    return book;
}

void order_book_destroy(OrderBook* book) {
    if (!book) {
        return;
    }

    LOG_INFO("Destroying order book");
    if (book->buy_orders) {
        avl_destroy(book->buy_orders);
    }
    if (book->sell_orders) {
        avl_destroy(book->sell_orders);
    }
    free(book);
}

static bool is_match_possible(const Order* buy_order, const Order* sell_order) {
    if (!buy_order || !sell_order) {
        LOG_ERROR("Attempted to match with NULL order(s)");
        return false;
    }

    if (buy_order->is_canceled || sell_order->is_canceled) {
        LOG_DEBUG("Match rejected: one or both orders are canceled");
        return false;
    }

    if (strcmp(buy_order->stock_symbol, sell_order->stock_symbol) != 0) {
        LOG_DEBUG("Match rejected: different symbols (%s vs %s)",
                 buy_order->stock_symbol, sell_order->stock_symbol);
        return false;
    }

    if (buy_order->price < sell_order->price) {
        LOG_DEBUG("Match rejected: buy price (%.2f) < sell price (%.2f)",
                 buy_order->price, sell_order->price);
        return false;
    }

    if (buy_order->remaining_quantity <= 0 || sell_order->remaining_quantity <= 0) {
        LOG_DEBUG("Match rejected: no remaining quantity");
        return false;
    }

    LOG_DEBUG("Match possible between buy order %s and sell order %s",
             buy_order->order_id, sell_order->order_id);
    return true;
}

static void process_match(Order* buy_order, Order* sell_order) {
    if (!buy_order || !sell_order) {
        LOG_ERROR("Attempted to process match with NULL order(s)");
        return;
    }

    int match_quantity = (buy_order->remaining_quantity < sell_order->remaining_quantity) ?
                        buy_order->remaining_quantity : sell_order->remaining_quantity;

    LOG_INFO("Processing match: Buy Order=%s, Sell Order=%s, Quantity=%d, Price=%.2f",
             buy_order->order_id, sell_order->order_id, match_quantity, sell_order->price);

    order_reduce_quantity(buy_order, match_quantity);
    order_reduce_quantity(sell_order, match_quantity);

    LOG_DEBUG("After match: Buy Order remaining=%d, Sell Order remaining=%d",
             buy_order->remaining_quantity, sell_order->remaining_quantity);
}

int order_book_add_order(OrderBook* book, Order* order) {
    if (!book || !order) {
        LOG_ERROR("Attempted to add NULL order to book");
        return -1;
    }

    LOG_INFO("Adding %s order to book: ID=%s, Symbol=%s, Price=%.2f, Quantity=%d",
             order->is_buy_order ? "buy" : "sell",
             order->order_id, order->stock_symbol,
             order->price, order->quantity);

    if (order->is_buy_order) {
        avl_insert(book->buy_orders, order->price, order->timestamp, order);
    } else {
        avl_insert(book->sell_orders, order->price, order->timestamp, order);
    }

    return 0;
}

void order_book_match_orders(OrderBook* book) {
    if (!book) {
        LOG_ERROR("Attempted to match orders in NULL book");
        return;
    }

    LOG_INFO("Starting order matching process");

    // Early exit if either buy or sell order tree is empty
    if (!book->buy_orders || !book->sell_orders ||
        avl_is_empty(book->buy_orders) || avl_is_empty(book->sell_orders)) {
        LOG_DEBUG("Cannot match orders: one or both sides are empty");
        return;
    }

    bool matches_found = true;
    int match_count = 0;

    while (matches_found) {
        matches_found = false;

        Order* best_buy = avl_find_max(book->buy_orders);
        Order* best_sell = avl_find_min(book->sell_orders);

        if (!best_buy || !best_sell) {
            LOG_DEBUG("No matching possible: one or both sides empty");
            break;
        }

        if (is_match_possible(best_buy, best_sell)) {
            process_match(best_buy, best_sell);
            matches_found = true;
            match_count++;

            // Remove fully matched orders
            if (best_buy->remaining_quantity == 0) {
                LOG_DEBUG("Removing fully matched buy order %s", best_buy->order_id);
                avl_delete_order(book->buy_orders, best_buy->price, best_buy->timestamp);
            }
            if (best_sell->remaining_quantity == 0) {
                LOG_DEBUG("Removing fully matched sell order %s", best_sell->order_id);
                avl_delete_order(book->sell_orders, best_sell->price, best_sell->timestamp);
            }
        }
    }

    LOG_INFO("Completed order matching process: %d matches executed", match_count);
}

int order_book_cancel_order(OrderBook* book, const char* order_id, bool is_buy_order) {
    if (!book || !order_id) {
        LOG_ERROR("Invalid parameters for order cancellation");
        return -1;
    }

    LOG_INFO("Attempting to cancel %s order: ID=%s",
             is_buy_order ? "buy" : "sell", order_id);

    // Note: In a real implementation, we would need a way to look up orders by ID
    // This might require maintaining a separate hash map of order IDs to orders
    // For now, we'll need to traverse the tree to find the order

    // Implementation needed: Find order by ID and cancel it
    LOG_WARN("Order cancellation by ID not fully implemented");
    return -1;
}

void order_book_traverse_buy_orders(const OrderBook* book, OrderCallback callback, void* user_data) {
    if (!book || !callback) {
        LOG_ERROR("Invalid parameters for buy orders traversal");
        return;
    }

    LOG_DEBUG("Starting buy orders traversal");
    avl_inorder_traverse(book->buy_orders, callback, user_data);
    LOG_DEBUG("Completed buy orders traversal");
}

void order_book_traverse_sell_orders(const OrderBook* book, OrderCallback callback, void* user_data) {
    if (!book || !callback) {
        LOG_ERROR("Invalid parameters for sell orders traversal");
        return;
    }

    LOG_DEBUG("Starting sell orders traversal");
    avl_inorder_traverse(book->sell_orders, callback, user_data);
    LOG_DEBUG("Completed sell orders traversal");
}

// Helper function to count quantity at a specific price level
struct PriceLevelData {
    double target_price;
    int total_quantity;
};

static void count_quantity_callback(Order* order, void* user_data) {
    struct PriceLevelData* data = (struct PriceLevelData*)user_data;
    if (order->price == data->target_price && !order->is_canceled) {
        data->total_quantity += order->remaining_quantity;
    }
}

int order_book_get_quantity_at_price(const OrderBook* book, double price, bool is_buy_order) {
    if (!book) {
        LOG_ERROR("Attempted to get quantity from NULL book");
        return 0;
    }

    struct PriceLevelData data = {
        .target_price = price,
        .total_quantity = 0
    };

    LOG_DEBUG("Calculating total quantity for %s orders at price %.2f",
             is_buy_order ? "buy" : "sell", price);

    if (is_buy_order) {
        avl_inorder_traverse(book->buy_orders, count_quantity_callback, &data);
    } else {
        avl_inorder_traverse(book->sell_orders, count_quantity_callback, &data);
    }

    LOG_DEBUG("Total quantity at price %.2f: %d", price, data.total_quantity);
    return data.total_quantity;
}
