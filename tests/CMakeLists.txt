cmake_minimum_required(VERSION 3.10)

# Trading engine tests
add_executable(test_trading_engine
    trading_engine/test_trading_engine.c
)

# Link tests with Unity and your library
target_link_libraries(test_trading_engine
    PRIVATE
    quant_trading_lib
    unity
)

target_include_directories(test_trading_engine
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/trading_engine
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/Unity/src
)

add_executable(test_order_loader
    utils/test_order_loader.c
)


target_link_libraries(test_order_loader
    PRIVATE
    quant_trading_lib
    unity
)

target_include_directories(test_order_loader
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/trading_engine
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/Unity/src
)

# Create test data directory in build directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/data)

# Copy test data files to build directory
file(COPY ${CMAKE_SOURCE_DIR}/tests/data/
     DESTINATION ${CMAKE_BINARY_DIR}/tests/data
     FILES_MATCHING PATTERN "test_orders*.txt"
                   PATTERN "test_orders*.csv")

# Add tests to CTest
enable_testing()
add_test(NAME test_trading_engine 
         COMMAND test_trading_engine
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
         
add_test(NAME test_order_loader 
         COMMAND test_order_loader
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
