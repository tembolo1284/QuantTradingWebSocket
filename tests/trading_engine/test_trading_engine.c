#include "unity.h"
#include "trading_engine/order_book.h"
#include "trading_engine/trader.h"
#include "trading_engine/order.h"
#include "trading_engine/trade.h"
#include "utils/logging.h"
#include <unistd.h>

// Test fixtures
OrderBook* book;
Trader* test_buyer;
Trader* test_seller;

void setUp(void) {
    LOG_INFO("Setting up test fixture");
    book = order_book_create();
    test_buyer = trader_create("TRADER1", "John Doe", 10000.0);
    test_seller = trader_create("TRADER2", "Jane Doe", 10000.0);
}

void tearDown(void) {
    LOG_INFO("Tearing down test fixture");
    order_book_destroy(book);
    trader_destroy(test_buyer);
    trader_destroy(test_seller);
}

// Basic order matching test
void test_basic_order_matching(void) {
    LOG_INFO("Starting basic order matching test");
    
    // Create orders
    Order* buy_order = order_create("BUY1", "TRADER1", "AAPL", 150.0, 100, true);
    Order* sell_order = order_create("SELL1", "TRADER2", "AAPL", 150.0, 100, false);

    // Place orders
    order_book_add_order(book, buy_order);
    order_book_add_order(book, sell_order);

    // Match orders
    order_book_match_orders(book);

    // Assert results
    TEST_ASSERT_EQUAL_INT(0, order_get_remaining_quantity(buy_order));
    TEST_ASSERT_EQUAL_INT(0, order_get_remaining_quantity(sell_order));

    // Cleanup
    order_destroy(buy_order);
    order_destroy(sell_order);
}

// Price priority test
void test_price_priority(void) {
    LOG_INFO("Starting price priority test");
    
    // Create multiple sell orders at different prices
    Order* sell1 = order_create("SELL1", "TRADER2", "AAPL", 152.0, 100, false);
    Order* sell2 = order_create("SELL2", "TRADER2", "AAPL", 151.0, 100, false);
    Order* sell3 = order_create("SELL3", "TRADER2", "AAPL", 150.0, 100, false);
    
    // Add sells in random order
    order_book_add_order(book, sell1);
    order_book_add_order(book, sell2);
    order_book_add_order(book, sell3);

    // Create buy order that should match with lowest price first
    Order* buy = order_create("BUY1", "TRADER1", "AAPL", 152.0, 100, true);
    order_book_add_order(book, buy);

    // Match orders
    order_book_match_orders(book);

    // Assert that lowest priced sell was matched first
    TEST_ASSERT_EQUAL_INT(0, order_get_remaining_quantity(sell3));  // 150.0 should be matched
    TEST_ASSERT_EQUAL_INT(100, order_get_remaining_quantity(sell1)); // 152.0 should not be matched

    // Cleanup
    order_destroy(sell1);
    order_destroy(sell2);
    order_destroy(sell3);
    order_destroy(buy);
}

// Time priority test
void test_time_priority(void) {
    LOG_INFO("Starting time priority test");
    
    // Create multiple sell orders at same price
    Order* sell1 = order_create("SELL1", "TRADER2", "AAPL", 150.0, 100, false);
    sleep(1); // Ensure different timestamps
    Order* sell2 = order_create("SELL2", "TRADER2", "AAPL", 150.0, 100, false);
    
    // Add orders
    order_book_add_order(book, sell1);
    order_book_add_order(book, sell2);

    // Create buy order that will match with one sell
    Order* buy = order_create("BUY1", "TRADER1", "AAPL", 150.0, 100, true);
    order_book_add_order(book, buy);

    // Match orders
    order_book_match_orders(book);

    // Assert earliest order was matched
    TEST_ASSERT_EQUAL_INT(0, order_get_remaining_quantity(sell1));   // First order should match
    TEST_ASSERT_EQUAL_INT(100, order_get_remaining_quantity(sell2)); // Second order should not

    // Cleanup
    order_destroy(sell1);
    order_destroy(sell2);
    order_destroy(buy);
}

// Order cancellation test
void test_order_cancellation(void) {
    LOG_INFO("Starting order cancellation test");
    
    Order* sell = order_create("SELL1", "TRADER2", "AAPL", 150.0, 100, false);
    Order* buy = order_create("BUY1", "TRADER1", "AAPL", 150.0, 100, true);
    
    order_book_add_order(book, sell);
    order_book_add_order(book, buy);

    // Cancel sell order
    order_cancel(sell);
    
    // Try to match
    order_book_match_orders(book);

    // Assert cancelled order wasn't matched
    TEST_ASSERT_TRUE(order_is_canceled(sell));
    TEST_ASSERT_EQUAL_INT(100, order_get_remaining_quantity(sell));
    TEST_ASSERT_EQUAL_INT(100, order_get_remaining_quantity(buy));

    // Cleanup
    order_destroy(sell);
    order_destroy(buy);
}

// Partial fill test
void test_partial_fills(void) {
    LOG_INFO("Starting partial fills test");
    
    Order* sell = order_create("SELL1", "TRADER2", "AAPL", 150.0, 100, false);
    Order* buy = order_create("BUY1", "TRADER1", "AAPL", 150.0, 50, true);
    
    order_book_add_order(book, sell);
    order_book_add_order(book, buy);

    order_book_match_orders(book);

    TEST_ASSERT_EQUAL_INT(50, order_get_remaining_quantity(sell)); // Should have 50 left
    TEST_ASSERT_EQUAL_INT(0, order_get_remaining_quantity(buy));   // Should be fully filled

    // Cleanup
    order_destroy(sell);
    order_destroy(buy);
}

// Balance update test
void test_balance_updates(void) {
    LOG_INFO("Starting balance updates test");
    
    double buyer_initial = trader_get_balance(test_buyer);
    (void) buyer_initial;
    double seller_initial = trader_get_balance(test_seller);

    Trade* trade = trade_create("BUY1", "SELL1", 150.0, 100);
    trade_execute(trade, test_buyer, test_seller);

    double expected_amount = 150.0 * 100;
    TEST_ASSERT_EQUAL_DOUBLE(seller_initial + expected_amount, trader_get_balance(test_seller));

    trade_destroy(trade);
}

// Multiple matches test
void test_multiple_matches(void) {
    LOG_INFO("Starting multiple matches test");
    
    // Create multiple buy and sell orders
    Order* buy1 = order_create("BUY1", "TRADER1", "AAPL", 150.0, 100, true);
    Order* buy2 = order_create("BUY2", "TRADER1", "AAPL", 149.0, 100, true);
    Order* sell1 = order_create("SELL1", "TRADER2", "AAPL", 148.0, 250, false);
    
    order_book_add_order(book, buy1);
    order_book_add_order(book, buy2);
    order_book_add_order(book, sell1);

    order_book_match_orders(book);

    TEST_ASSERT_EQUAL_INT(0, order_get_remaining_quantity(buy1));    // Should be fully matched
    TEST_ASSERT_EQUAL_INT(50, order_get_remaining_quantity(sell1));  // Should have 50 left
    TEST_ASSERT_EQUAL_INT(0, order_get_remaining_quantity(buy2));    // Should be fully matched

    // Cleanup
    order_destroy(buy1);
    order_destroy(buy2);
    order_destroy(sell1);
}

int main(void) {
    set_log_level(LOG_INFO);
    LOG_INFO("Starting trading system tests");
    
    UNITY_BEGIN();
    
    RUN_TEST(test_basic_order_matching);
    RUN_TEST(test_price_priority);
    RUN_TEST(test_time_priority);
    RUN_TEST(test_order_cancellation);
    RUN_TEST(test_partial_fills);
    RUN_TEST(test_balance_updates);
    RUN_TEST(test_multiple_matches);
    
    LOG_INFO("All tests completed");
    return UNITY_END();
}
