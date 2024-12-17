# Only include tests for files that exist
set(TEST_FILES
    net/test_frame.c
    net/test_handshake.c
    net/test_socket.c
)

# Create test executables
foreach(test_source ${TEST_FILES})
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
