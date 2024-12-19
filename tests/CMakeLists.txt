# Group test files by component.
set(NET_TEST_FILES
net/test_frame.c
net/test_handshake.c
net/test_socket.c
)
set(TRADING_ENGINE_TEST_FILES
trading/engine/test_order.c
trading/engine/test_order_book.c
trading/engine/test_matcher.c
)
set(TRADING_PROTOCOL_TEST_FILES
trading/protocol/test_messages.c
)
set(CLIENT_TEST_FILES
client/test_client_handler.c
)
set(SERVER_TEST_FILES
server/test_request_handler.c
)

# Combine all test files
set(ALL_TEST_FILES
${NET_TEST_FILES}
${TRADING_ENGINE_TEST_FILES}
${TRADING_PROTOCOL_TEST_FILES}
${CLIENT_TEST_FILES}
${SERVER_TEST_FILES}
)

# Create test executables
foreach(test_source ${ALL_TEST_FILES})
    # Only create test if source file exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_source})
        get_filename_component(test_name ${test_source} NAME_WE)
        add_executable(${test_name} ${test_source})
        
        # Link appropriate libraries based on test type
        if(test_source MATCHES "^client/")
            target_link_libraries(${test_name} PRIVATE client_lib)
        elseif(test_source MATCHES "^server/")
            target_link_libraries(${test_name} PRIVATE server_lib)
        endif()
        
        # Common libraries for all tests
        target_link_libraries(${test_name}
            PRIVATE
            quant_trading_lib
            unity
        )
        
        add_test(NAME ${test_name} COMMAND ${test_name})
        list(APPEND TEST_TARGETS ${test_name})
    endif()
endforeach()

# Create test suites by component
add_custom_target(test_net
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --regexp "test_.*_net"
    DEPENDS ${TEST_TARGETS}
)

add_custom_target(test_trading
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --regexp "test_.*_trading"
    DEPENDS ${TEST_TARGETS}
)

add_custom_target(test_client
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --regexp "test_.*_client"
    DEPENDS ${TEST_TARGETS}
)

add_custom_target(test_server
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --regexp "test_.*_server"
    DEPENDS ${TEST_TARGETS}
)

# Main test target that runs all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_TARGETS}
)
