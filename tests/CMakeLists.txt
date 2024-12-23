cmake_minimum_required(VERSION 3.10)

# Trading engine tests
add_executable(test_order
    trading/engine/test_order.c
)

add_executable(test_order_book
    trading/engine/test_order_book.c
)

# Link tests with Unity and your library
target_link_libraries(test_order
    quant_trading_lib
    unity
)

target_link_libraries(test_order_book
    quant_trading_lib
    unity
)

# Add tests to CTest
enable_testing()
add_test(NAME test_order COMMAND test_order)
add_test(NAME test_order_book COMMAND test_order_book)
