#include "trading/engine/order.h"

#define UNITY_DOUBLE_PRECISION 0.00001
#define UNITY_DOUBLE_COMPARE_DELTA 0.00001

#include "unity.h"
#include "unity_config.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
    // Setup code if needed
}

void tearDown(void) {
    // Cleanup code if needed
}

void test_order_create_valid() {
    const char* symbol = "AAPL";
    double price = 150.50;
    uint32_t quantity = 100;
    bool is_buy = true;

    Order* order = order_create(symbol, price, quantity, is_buy);
    TEST_ASSERT_NOT_NULL(order);
    TEST_ASSERT_EQUAL_STRING(symbol, order->symbol);
    TEST_ASSERT_EQUAL_DOUBLE(price, order->price);
    TEST_ASSERT_EQUAL_UINT32(quantity, order->quantity);
    TEST_ASSERT_TRUE(order->is_buy);
    TEST_ASSERT_NOT_EQUAL(0, order->id);
    TEST_ASSERT_NOT_EQUAL(0, order->timestamp);

    free(order);
}

void test_order_create_invalid_params() {
    // Test NULL symbol
    Order* order = order_create(NULL, 100.0, 100, true);
    TEST_ASSERT_NULL(order);

    // Test empty symbol
    order = order_create("", 100.0, 100, true);
    TEST_ASSERT_NULL(order);

    // Test zero price
    order = order_create("AAPL", 0.0, 100, true);
    TEST_ASSERT_NULL(order);

    // Test negative price
    order = order_create("AAPL", -100.0, 100, true);
    TEST_ASSERT_NULL(order);

    // Test zero quantity
    order = order_create("AAPL", 100.0, 0, true);
    TEST_ASSERT_NULL(order);
}

void test_order_validate() {
    Order valid_order = {
        .id = 1,
        .symbol = "AAPL",
        .price = 150.50,
        .quantity = 100,
        .timestamp = get_timestamp(),
        .is_buy = true
    };

    TEST_ASSERT_TRUE(order_validate(&valid_order));

    // Test invalid price
    Order invalid_price = valid_order;
    invalid_price.price = 0.0;
    TEST_ASSERT_FALSE(order_validate(&invalid_price));

    // Test invalid quantity
    Order invalid_quantity = valid_order;
    invalid_quantity.quantity = 0;
    TEST_ASSERT_FALSE(order_validate(&invalid_quantity));

    // Test empty symbol
    Order invalid_symbol = valid_order;
    invalid_symbol.symbol[0] = '\0';
    TEST_ASSERT_FALSE(order_validate(&invalid_symbol));

    // Test future timestamp
    Order invalid_timestamp = valid_order;
    invalid_timestamp.timestamp = get_timestamp() + 1000000;  // 1 second in future
    TEST_ASSERT_FALSE(order_validate(&invalid_timestamp));
}

void test_order_id_uniqueness() {
    Order* order1 = order_create("AAPL", 150.50, 100, true);
    Order* order2 = order_create("AAPL", 150.50, 100, true);
    Order* order3 = order_create("AAPL", 150.50, 100, true);

    TEST_ASSERT_NOT_NULL(order1);
    TEST_ASSERT_NOT_NULL(order2);
    TEST_ASSERT_NOT_NULL(order3);

    TEST_ASSERT_NOT_EQUAL(order1->id, order2->id);
    TEST_ASSERT_NOT_EQUAL(order2->id, order3->id);
    TEST_ASSERT_NOT_EQUAL(order1->id, order3->id);

    free(order1);
    free(order2);
    free(order3);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_order_create_valid);
    RUN_TEST(test_order_create_invalid_params);
    RUN_TEST(test_order_validate);
    RUN_TEST(test_order_id_uniqueness);
    
    return UNITY_END();
}
