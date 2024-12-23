#include "trading/engine/matcher.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

#define MAX_SYMBOLS 100

// Structure to hold multiple order books
typedef struct {
    OrderBook* book;
    char symbol[16];
    bool active;
} BookEntry;

// Array of order books
static BookEntry order_books[MAX_SYMBOLS] = {0};
static size_t active_books = 0;

bool order_handler_init(void) {
    // Initialize all book entries
    for (size_t i = 0; i < MAX_SYMBOLS; i++) {
        order_books[i].book = NULL;
        order_books[i].symbol[0] = '\0';
        order_books[i].active = false;
    }
    active_books = 0;
    return true;
}

void order_handler_shutdown(void) {
    // Cleanup all active books
    for (size_t i = 0; i < MAX_SYMBOLS; i++) {
        if (order_books[i].active) {
            LOG_DEBUG("Destroying order book for symbol: %s", order_books[i].symbol);
            order_book_destroy(order_books[i].book);
            order_books[i].book = NULL;
            order_books[i].symbol[0] = '\0';
            order_books[i].active = false;
        }
    }
    active_books = 0;
    LOG_INFO("Order handler shutdown complete");
}

static BookEntry* find_book_entry(const char* symbol) {
    for (size_t i = 0; i < MAX_SYMBOLS; i++) {
        if (order_books[i].active && strcmp(order_books[i].symbol, symbol) == 0) {
            return &order_books[i];
        }
    }
    return NULL;
}

static BookEntry* get_free_book_entry(void) {
    for (size_t i = 0; i < MAX_SYMBOLS; i++) {
        if (!order_books[i].active) {
            return &order_books[i];
        }
    }
    return NULL;
}

static void validate_books_state(void) {
    size_t actual_count = 0;
    bool issues_found = false;

    for (size_t i = 0; i < MAX_SYMBOLS; i++) {
        if (order_books[i].active) {
            actual_count++;
            
            if (!order_books[i].book) {
                LOG_ERROR("Active book entry %zu has NULL book pointer", i);
                issues_found = true;
                continue;
            }
            
            if (order_books[i].symbol[0] == '\0') {
                LOG_ERROR("Active book entry %zu has empty symbol", i);
                issues_found = true;
            }
            
            if (strcmp(order_books[i].book->symbol, order_books[i].symbol) != 0) {
                LOG_ERROR("Book symbol mismatch at %zu: entry=%s, book=%s", 
                         i, order_books[i].symbol, order_books[i].book->symbol);
                issues_found = true;
            }
        }
    }

    if (actual_count != active_books) {
        LOG_ERROR("Active books count mismatch: tracked=%zu, actual=%zu", 
                 active_books, actual_count);
        issues_found = true;
    }

    if (issues_found) {
        LOG_WARN("Order book state validation failed");
    } else {
        LOG_DEBUG("Order book state validation passed (%zu active books)", active_books);
    }
}

bool order_handler_create_book(const char* symbol) {
    // Require symbol to be provided
    if (!symbol || symbol[0] == '\0') {
        LOG_ERROR("Symbol must be provided when creating order book");
        return false;
    }

    LOG_DEBUG("Attempting to create/find book for symbol: %s (Active books: %zu)", 
              symbol, active_books);

    // Check if book already exists
    BookEntry* entry = find_book_entry(symbol);
    if (entry) {
        LOG_DEBUG("Using existing order book for symbol: %s (Total orders: %lu)", 
                  symbol, entry->book->total_orders);
        return true;
    }

    // Find space for new book
    if (active_books >= MAX_SYMBOLS) {
        LOG_ERROR("Maximum number of order books reached (%d)", MAX_SYMBOLS);
        return false;
    }

    entry = get_free_book_entry();
    if (!entry) {
        LOG_ERROR("No free slots available for new order book");
        return false;
    }

    // Create new order book
    entry->book = order_book_create(symbol);
    if (!entry->book) {
        LOG_ERROR("Failed to create order book for symbol: %s", symbol);
        return false;
    }

    // Initialize entry
    strncpy(entry->symbol, symbol, sizeof(entry->symbol) - 1);
    entry->symbol[sizeof(entry->symbol) - 1] = '\0';
    entry->active = true;
    active_books++;

    LOG_INFO("Order book created for symbol: %s (Total active books: %zu)", 
             symbol, active_books);
    
    // Debug log all active books
    LOG_DEBUG("Current active books:");
    for (size_t i = 0; i < MAX_SYMBOLS; i++) {
        if (order_books[i].active) {
            LOG_DEBUG("  - %s (Orders: %lu)", 
                     order_books[i].symbol, 
                     order_books[i].book->total_orders);
        }
    }
    
    return true;
}

OrderHandlingResult order_handler_add_order(const Order* order) {
    validate_books_state();
    if (!order) {
        LOG_ERROR("Invalid order (NULL)");
        validate_books_state();
        return ORDER_INVALID;
    }

    LOG_DEBUG("Attempting to add order: symbol=%s, price=%.2f, quantity=%u, is_buy=%d",
             order->symbol, order->price, order->quantity, order->is_buy);

    // Find or create book for symbol
    if (!order_handler_create_book(order->symbol)) {
        validate_books_state();
        return ORDER_INVALID;
    }

    BookEntry* entry = find_book_entry(order->symbol);
    if (!entry || !entry->book) {
        LOG_ERROR("Failed to get order book for symbol: %s", order->symbol);
        validate_books_state();
        return ORDER_INVALID;
    }

    // Validate price and quantity
    if (order->price <= 0.0 || order->quantity == 0) {
        LOG_ERROR("Invalid price (%.2f) or quantity (%u)",
                  order->price, order->quantity);
        validate_books_state();
        return ORDER_INVALID;
    }

    // Add order to book
    if (order_book_add(entry->book, order)) {
        LOG_INFO("Order added successfully: id=%lu, price=%.2f, quantity=%u, is_buy=%d",
                 order->id, order->price, order->quantity, order->is_buy);
        validate_books_state();
        return ORDER_SUCCESS;
    } else {
        LOG_ERROR("Failed to add order to order book");
        validate_books_state();
        return ORDER_REJECTED;
    }
}

// Get order book for a specific symbol
OrderBook* order_handler_get_book_by_symbol(const char* symbol) {
    BookEntry* entry = find_book_entry(symbol);
    return entry ? entry->book : NULL;
}

// Legacy function for backward compatibility
OrderBook* order_handler_get_book(void) {
    // Return first active book or NULL
    for (size_t i = 0; i < MAX_SYMBOLS; i++) {
        if (order_books[i].active) {
            return order_books[i].book;
        }
    }
    return NULL;
}

size_t order_handler_get_all_books(OrderBook** books, size_t max_books) {
    validate_books_state();
    size_t count = 0;
    LOG_DEBUG("Getting all active books (max: %zu)", max_books);

    for (size_t i = 0; i < MAX_SYMBOLS && count < max_books; i++) {
        if (order_books[i].active && order_books[i].book) {
            LOG_DEBUG("Validating book: %s", order_books[i].symbol);
            if (!order_books[i].book->sell_tree) {
                LOG_WARN("Sell tree for book %s is NULL", order_books[i].symbol);
            }
            if (!order_books[i].book->buy_tree) {
                LOG_WARN("Buy tree for book %s is NULL", order_books[i].symbol);
            }
            books[count++] = order_books[i].book;
        }
    }

    LOG_DEBUG("Returning %zu active books", count);
    return count;
}

// Get current active book count
size_t order_handler_get_active_book_count(void) {
    size_t real_count = 0;
    for (size_t i = 0; i < MAX_SYMBOLS; i++) {
        if (order_books[i].active && order_books[i].book) {
            real_count++;
        }
    }
    
    if (real_count != active_books) {
        LOG_WARN("Active books count mismatch: tracked=%zu, actual=%zu", 
                 active_books, real_count);
        active_books = real_count;  // Fix the count
    }
    
    return active_books;
}
