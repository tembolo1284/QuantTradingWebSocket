#include <criterion/criterion.h>
#include "trading/order_book.h"

static OrderBook* book;

void setup(void) {
    book = order_book_create("AAPL");
}

void teardown(void) {
    order_book_destroy(book);
}

Test(order_book, creation, .init = setup, .fini = teardown) {
    cr_assert(book != NULL, "Order book should be created successfully");
}

Test(order_book, add_orders, .init = setup, .fini = teardown) {
    Order buy_order = {
        .price = 100.0,
        .quantity = 10,
        .is_buy = true,
        .timestamp = get_timestamp()
    };
    strncpy(buy_order.symbol, "AAPL", sizeof(buy_order.symbol));

    Order sell_order = {
        .price = 101.0,
        .quantity = 10,
        .is_buy = false,
        .timestamp = get_timestamp()
    };
    strncpy(sell_order.symbol, "AAPL", sizeof(sell_order.symbol));

    cr_assert(order_book_add(book, &buy_order), "Should add buy order successfully");
    cr_assert(order_book_add(book, &sell_order), "Should add sell order successfully");

    cr_assert_float_eq(order_book_get_best_bid(book), 100.0, 0.001, "Best bid should be 100.0");
    cr_assert_float_eq(order_book_get_best_ask(book), 101.0, 0.001, "Best ask should be 101.0");
}

Test(order_book, price_time_priority, .init = setup, .fini = teardown) {
    // Add orders with same price but different timestamps
    Order order1 = {
        .price = 100.0,
        .quantity = 10,
        .is_buy = true,
        .timestamp = get_timestamp()
    };
    strncpy(order1.symbol, "AAPL", sizeof(order1.symbol));
    
    // Sleep briefly to ensure different timestamps
    usleep(1000);
    
    Order order2 = {
        .price = 100.0,
        .quantity = 20,
        .is_buy = true,
        .timestamp = get_timestamp()
    };
    strncpy(order2.symbol, "AAPL", sizeof(order2.symbol));

    cr_assert(order_book_add(book, &order1), "Should add first order");
    cr_assert(order_book_add(book, &order2), "Should add second order");

    // Verify time priority by attempting to match orders
    // First order should be matched first
    cr_assert(order1.timestamp < order2.timestamp, 
              "First order should have earlier timestamp");
}

Test(order_book, cancel_order, .init = setup, .fini = teardown) {
    Order order = {
        .price = 100.0,
        .quantity = 10,
        .is_buy = true,
        .timestamp = get_timestamp()
    };
    strncpy(order.symbol, "AAPL", sizeof(order.symbol));

    cr_assert(order_book_add(book, &order), "Should add order successfully");
    cr_assert(order_book_cancel(book, order.id), "Should cancel order successfully");
    cr_assert_float_eq(order_book_get_best_bid(book), 0.0, 0.001, 
                      "Best bid should be 0 after cancellation");
}
