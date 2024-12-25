#include "unity.h"
#include "trading_engine/order_book.h"
#include "trading_engine/order.h"
#include "utils/order_loader.h"
#include "utils/logging.h"
#include <stdlib.h>

OrderBook* book;

void setUp(void) {
    book = order_book_create();
}

void tearDown(void) {
    order_book_destroy(book);
}

// Callback function for counting orders
static void count_callback(Order* order, void* data) {
    size_t* counter = (size_t*)data;
    (*counter)++;
}

// Helper function to count orders in the book
static size_t count_orders(const OrderBook* book, bool is_buy_order) {
    size_t count = 0;
    
    if (is_buy_order) {
        order_book_traverse_buy_orders(book, count_callback, &count);
    } else {
        order_book_traverse_sell_orders(book, count_callback, &count);
    }
    
    return count;
}

void test_load_orders_from_csv(void) {
    // Load orders from CSV file
    int loaded = load_orders_from_file("tests/data/test_orders.csv", book);
    TEST_ASSERT_GREATER_THAN(0, loaded);
    
    // Count orders in the book
    size_t buy_count = count_orders(book, true);
    size_t sell_count = count_orders(book, false);
    
    // Verify we have a mix of buy and sell orders
    TEST_ASSERT_GREATER_THAN(0, buy_count);
    TEST_ASSERT_GREATER_THAN(0, sell_count);
    TEST_ASSERT_EQUAL_INT(loaded, buy_count + sell_count);
    
    // Test order matching
    order_book_match_orders(book);
    
    // Verify some orders were matched
    size_t remaining_buy_count = count_orders(book, true);
    size_t remaining_sell_count = count_orders(book, false);
    
    TEST_ASSERT_LESS_THAN(buy_count, remaining_buy_count);
    TEST_ASSERT_LESS_THAN(sell_count, remaining_sell_count);
}

void test_load_orders_from_txt(void) {
    // Load orders from TXT file
    int loaded = load_orders_from_file("data/test_orders.txt", book);
    TEST_ASSERT_GREATER_THAN(0, loaded);
    
    // Count orders in the book
    size_t buy_count = count_orders(book, true);
    size_t sell_count = count_orders(book, false);
    
    // Verify we have orders from both sides
    TEST_ASSERT_GREATER_THAN(0, buy_count);
    TEST_ASSERT_GREATER_THAN(0, sell_count);
    TEST_ASSERT_EQUAL_INT(loaded, buy_count + sell_count);
}

void test_load_orders_invalid_file(void) {
    int result = load_orders_from_file("nonexistent_file.csv", book);
    TEST_ASSERT_EQUAL_INT(-1, result);
    
    result = load_orders_from_file("invalid_extension.xyz", book);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_load_orders_null_params(void) {
    int result = load_orders_from_file(NULL, book);
    TEST_ASSERT_EQUAL_INT(-1, result);
    
    result = load_orders_from_file("data/test_orders.csv", NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_load_orders_malformed_lines(void) {
    int loaded = load_orders_from_file("data/test_orders_malformed.txt", book);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, loaded);  // Should skip malformed lines but continue processing
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_load_orders_from_csv);
    RUN_TEST(test_load_orders_from_txt);
    RUN_TEST(test_load_orders_invalid_file);
    RUN_TEST(test_load_orders_null_params);
    RUN_TEST(test_load_orders_malformed_lines);
    
    return UNITY_END();
}
