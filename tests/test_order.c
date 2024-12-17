#include "unity.h"
#include "trading/order.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {
    // No setup needed
}

void tearDown(void) {
    // No teardown needed
}

void test_order_creation(void) {
    Order* order = order_create("AAPL", 150.0, 100, true);
    TEST_ASSERT_NOT_NULL(order);
    TEST_ASSERT_EQUAL_STRING("AAPL", order->symbol);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 150.0, order->price);
    TEST_ASSERT_EQUAL_UINT32(100, order->quantity);
    TEST_ASSERT_TRUE(order->is_buy);
    free(order);
}

void test_order_validation(void) {
    // Valid order
    Order valid_order = {
        .price = 100.0,
        .quantity = 10,
        .is_buy = true,
        .timestamp = get_timestamp()
    };
    strncpy(valid_order.symbol, "AAPL", sizeof(valid_order.symbol));
    
    TEST_ASSERT_TRUE(order_validate(&valid_order));

    // Invalid price
    Order invalid_price = valid_order;
    invalid_price.price = 0.0;
    TEST_ASSERT_FALSE(order_validate(&invalid_price));

    // Invalid quantity
    Order invalid_quantity = valid_order;
    invalid_quantity.quantity = 0;
    TEST_ASSERT_FALSE(order_validate(&invalid_quantity));

    // Invalid symbol
    Order invalid_symbol = valid_order;
    invalid_symbol.symbol[0] = '\0';
    TEST_ASSERT_FALSE(order_validate(&invalid_symbol));

    // Future timestamp
    Order future_timestamp = valid_order;
    future_timestamp.timestamp = UINT64_MAX;
    TEST_ASSERT_FALSE(order_validate(&future_timestamp));
}

void test_symbol_bounds(void) {
    Order* order = order_create("AAPLGOOGLEAMAZON", 150.0, 100, true);
    TEST_ASSERT_NOT_NULL(order);
    TEST_ASSERT_EQUAL_UINT(sizeof(order->symbol) - 1, strlen(order->symbol));
    free(order);
}

void test_unique_ids(void) {
    Order* order1 = order_create("AAPL", 150.0, 100, true);
    Order* order2 = order_create("AAPL", 150.0, 100, true);
    
    TEST_ASSERT_NOT_EQUAL(order1->id, order2->id);
    
    free(order1);
    free(order2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_order_creation);
    RUN_TEST(test_order_validation);
    RUN_TEST(test_symbol_bounds);
    RUN_TEST(test_unique_ids);
    return UNITY_END();
}
