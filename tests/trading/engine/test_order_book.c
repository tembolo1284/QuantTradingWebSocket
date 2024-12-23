#include "trading/engine/order_book.h"

#define UNITY_DOUBLE_PRECISION 0.00001
#define UNITY_DOUBLE_COMPARE_DELTA 0.00001

#include "unity.h"
#include "unity_config.h"
#include "utils/logging.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static Trade last_trade;
static bool trade_executed;

void setUp(void) {
    trade_executed = false;
    memset(&last_trade, 0, sizeof(Trade));
}

void tearDown(void) {
    // Cleanup code if needed
}

static void on_trade(const Trade* trade, void* user_data) {
    (void)user_data;  // Unused parameter
    memcpy(&last_trade, trade, sizeof(Trade));
    trade_executed = true;
}

void test_order_book_create() {
    OrderBook* book = order_book_create("AAPL");
    TEST_ASSERT_NOT_NULL(book);
    TEST_ASSERT_EQUAL_STRING("AAPL", order_book_get_symbol(book));
    TEST_ASSERT_EQUAL_size_t(0, order_book_get_order_count(book));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_bid(book));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_ask(book));
    order_book_destroy(book);
}

void test_order_book_add_orders() {
    OrderBook* book = order_book_create("AAPL");
    TEST_ASSERT_NOT_NULL(book);

    // Add buy order
    Order buy_order = {
        .id = 1,
        .symbol = "AAPL",
        .price = 150.50,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = true
    };
    TEST_ASSERT_TRUE(order_book_add(book, &buy_order));
    TEST_ASSERT_EQUAL_DOUBLE(150.50, order_book_get_best_bid(book));

    // Add sell order
    Order sell_order = {
        .id = 2,
        .symbol = "AAPL",
        .price = 151.00,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = false
    };
    TEST_ASSERT_TRUE(order_book_add(book, &sell_order));
    TEST_ASSERT_EQUAL_DOUBLE(151.00, order_book_get_best_ask(book));

    TEST_ASSERT_EQUAL_size_t(2, order_book_get_order_count(book));
    order_book_destroy(book);
}

void test_order_matching_exact() {
    OrderBook* book = order_book_create("AAPL");
    order_book_set_trade_callback(book, on_trade, NULL);

    // Add buy order first
    Order buy_order = {
        .id = 1,
        .symbol = "AAPL",
        .price = 150.00,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = true
    };
    TEST_ASSERT_TRUE(order_book_add(book, &buy_order));
    LOG_INFO("After buy order: best_bid = %.2f, best_ask = %.2f", 
             order_book_get_best_bid(book), order_book_get_best_ask(book));

    // Add matching sell order
    Order sell_order = {
        .id = 2,
        .symbol = "AAPL",
        .price = 150.00,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = false
    };
    TEST_ASSERT_TRUE(order_book_add(book, &sell_order));
    LOG_INFO("After sell order: best_bid = %.2f, best_ask = %.2f", 
             order_book_get_best_bid(book), order_book_get_best_ask(book));

    // Verify trade
    TEST_ASSERT_TRUE(trade_executed);
    TEST_ASSERT_EQUAL_UINT64(1, last_trade.buy_order_id);
    TEST_ASSERT_EQUAL_UINT64(2, last_trade.sell_order_id);
    TEST_ASSERT_EQUAL_DOUBLE(150.00, last_trade.price);
    TEST_ASSERT_EQUAL_UINT32(100, last_trade.quantity);

    // Verify order book state
    TEST_ASSERT_EQUAL_size_t(0, order_book_get_order_count(book));
    LOG_INFO("Final state: best_bid = %.2f, best_ask = %.2f", 
             order_book_get_best_bid(book), order_book_get_best_ask(book));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_bid(book));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_ask(book));

    order_book_destroy(book);
}

void test_order_matching_partial() {
    OrderBook* book = order_book_create("AAPL");
    order_book_set_trade_callback(book, on_trade, NULL);

    // Add larger buy order first
    Order buy_order = {
        .id = 1,
        .symbol = "AAPL",
        .price = 150.00,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = true
    };
    TEST_ASSERT_TRUE(order_book_add(book, &buy_order));

    // Add smaller sell order
    Order sell_order = {
        .id = 2,
        .symbol = "AAPL",
        .price = 150.00,
        .quantity = 60,
        .timestamp = get_timestamp(),
        .is_buy = false
    };
    TEST_ASSERT_TRUE(order_book_add(book, &sell_order));

    // Verify trade
    TEST_ASSERT_TRUE(trade_executed);
    TEST_ASSERT_EQUAL_UINT64(1, last_trade.buy_order_id);
    TEST_ASSERT_EQUAL_UINT64(2, last_trade.sell_order_id);
    TEST_ASSERT_EQUAL_DOUBLE(150.00, last_trade.price);
    TEST_ASSERT_EQUAL_UINT32(60, last_trade.quantity);

    // Verify order book state - should have remaining buy order
    TEST_ASSERT_EQUAL_size_t(1, order_book_get_order_count(book));
    TEST_ASSERT_EQUAL_DOUBLE(150.00, order_book_get_best_bid(book));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_ask(book));

    order_book_destroy(book);
}

void test_order_cancellation() {
    OrderBook* book = order_book_create("AAPL");

    // Add order to cancel
    Order order = {
        .id = 1,
        .symbol = "AAPL",
        .price = 150.00,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = true
    };
    TEST_ASSERT_TRUE(order_book_add(book, &order));
    TEST_ASSERT_EQUAL_size_t(1, order_book_get_order_count(book));

    // Test successful cancellation
    TEST_ASSERT_EQUAL(CANCEL_SUCCESS, order_book_cancel(book, 1));
    TEST_ASSERT_EQUAL_size_t(0, order_book_get_order_count(book));

    // Test cancelling non-existent order
    TEST_ASSERT_EQUAL(CANCEL_ORDER_NOT_FOUND, order_book_cancel(book, 999));

    order_book_destroy(book);
}

void test_price_time_priority() {
    OrderBook* book = order_book_create("AAPL");
    order_book_set_trade_callback(book, on_trade, NULL);

    // Add two buy orders at same price
    Order buy_order1 = {
        .id = 1,
        .symbol = "AAPL",
        .price = 150.00,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = true
    };
    TEST_ASSERT_TRUE(order_book_add(book, &buy_order1));
    
    usleep(50000);

    Order buy_order2 = {
        .id = 2,
        .symbol = "AAPL",
        .price = 150.00,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = true
    };
    TEST_ASSERT_TRUE(order_book_add(book, &buy_order2));

    usleep(50000);

    // Add matching sell order
    Order sell_order = {
        .id = 3,
        .symbol = "AAPL",
        .price = 150.00,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = false
    };
    TEST_ASSERT_TRUE(order_book_add(book, &sell_order));

    // Verify first order was matched (time priority)
    TEST_ASSERT_TRUE(trade_executed);
    TEST_ASSERT_EQUAL_UINT64(1, last_trade.buy_order_id);
    TEST_ASSERT_EQUAL_UINT64(3, last_trade.sell_order_id);

    order_book_destroy(book);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_order_book_create);
    RUN_TEST(test_order_book_add_orders);
    RUN_TEST(test_order_matching_exact);
    RUN_TEST(test_order_matching_partial);
    RUN_TEST(test_order_cancellation);
    RUN_TEST(test_price_time_priority);
    
    return UNITY_END();
}
