#include "unity.h"
#include "trading/order_book.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static OrderBook* book;

void setUp(void) {
    book = order_book_create("AAPL");
}

void tearDown(void) {
    order_book_destroy(book);
}

void test_order_book_creation(void) {
    TEST_ASSERT_NOT_NULL(book);
}

void test_add_orders(void) {
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

    TEST_ASSERT_TRUE(order_book_add(book, &buy_order));
    TEST_ASSERT_TRUE(order_book_add(book, &sell_order));

    TEST_ASSERT_FLOAT_WITHIN(0.001, 100.0, order_book_get_best_bid(book));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 101.0, order_book_get_best_ask(book));
}

void test_price_time_priority(void) {
    Order order1 = {
        .price = 100.0,
        .quantity = 10,
        .is_buy = true,
        .timestamp = get_timestamp()
    };
    strncpy(order1.symbol, "AAPL", sizeof(order1.symbol));
    
    usleep(1000);  // Ensure different timestamps
    
    Order order2 = {
        .price = 100.0,
        .quantity = 20,
        .is_buy = true,
        .timestamp = get_timestamp()
    };
    strncpy(order2.symbol, "AAPL", sizeof(order2.symbol));

    TEST_ASSERT_TRUE(order_book_add(book, &order1));
    TEST_ASSERT_TRUE(order_book_add(book, &order2));
    TEST_ASSERT_TRUE(order1.timestamp < order2.timestamp);
}

void test_cancel_order(void) {
    Order order = {
        .price = 100.0,
        .quantity = 10,
        .is_buy = true,
        .timestamp = get_timestamp()
    };
    strncpy(order.symbol, "AAPL", sizeof(order.symbol));

    TEST_ASSERT_TRUE(order_book_add(book, &order));
    TEST_ASSERT_TRUE(order_book_cancel(book, order.id));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, order_book_get_best_bid(book));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_order_book_creation);
    RUN_TEST(test_add_orders);
    RUN_TEST(test_price_time_priority);
    RUN_TEST(test_cancel_order);
    return UNITY_END();
}
