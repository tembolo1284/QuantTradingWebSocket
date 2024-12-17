# Network test files
set(NET_TESTS
    net/test_websocket.c
    net/test_frame.c
    net/test_handshake.c
    net/test_socket.c
    net/test_buffer.c
)

# Trading test files
set(TRADING_TESTS
    trading/test_order.c
    trading/test_order_book.c
)

# Create test executables for network components
foreach(test_source ${NET_TESTS})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name}
        PRIVATE
        quant_trading_lib
        unity
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Create test executables for trading components
foreach(test_source ${TRADING_TESTS})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name}
        PRIVATE
        quant_trading_lib
        unity
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Test suite setup
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_TARGETS}
)
