cmake_minimum_required(VERSION 3.10)

option(USE_CLANG "Use Clang as the compiler" OFF)

if(USE_CLANG)
    find_program(CLANG_C_COMPILER clang REQUIRED)
    if(CLANG_C_COMPILER)
        set(CMAKE_C_COMPILER "${CLANG_C_COMPILER}")
        message(STATUS "Using Clang as the C compiler: ${CLANG_C_COMPILER}")
    else()
        message(FATAL_ERROR "Clang compiler not found. Please install Clang or unset USE_CLANG.")
    endif()
else()
    message(STATUS "Using default C compiler: ${CMAKE_C_COMPILER}")
endif()

project(quant_trading C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-field-initializers -Wno-gnu-zero-variadic-macro-arguments")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU")
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -march=native")

option(BUILD_TESTS "Build tests" ON)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_CJSON_TEST "Enable cJSON tests" OFF)
option(ENABLE_CJSON_UTILS "Enable cJSON utils" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Dependencies
add_subdirectory(third_party/cJSON)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(LibWebSockets REQUIRED)

# Unity for testing
if(BUILD_TESTS)
   if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/Unity/src/unity.c)
       set(UNITY_ROOT ${CMAKE_SOURCE_DIR}/third_party/Unity)
       add_library(unity STATIC
           ${UNITY_ROOT}/src/unity.c
       )
       target_include_directories(unity PUBLIC
           ${UNITY_ROOT}/src
           ${CMAKE_SOURCE_DIR}/third_party/Unity/src
       )
       target_compile_definitions(unity PUBLIC
           UNITY_INCLUDE_DOUBLE
           UNITY_DOUBLE_PRECISION=0.00001
       )
       set(UNITY_FOUND TRUE)
   else()
       message(WARNING "Unity not found in third_party/Unity - tests will be disabled")
       set(BUILD_TESTS OFF)
   endif()
endif()

set(TRADING_ENGINE_SOURCES
    src/trading_engine/order.c
    src/trading_engine/order_book.c
    src/trading_engine/trade.c
    src/trading_engine/trader.c
    src/trading_engine/avl_tree.c
)

set(UTILS_SOURCES
    src/utils/logging.c
    src/utils/order_loader.c
)

set(PROTOCOL_SOURCES
    src/protocol/json_protocol.c
    src/protocol/protocol_validation.c
)

set(SERVER_SOURCES
    src/server/market_data.c
    src/server/server_handlers.c
    src/server/session_manager.c
    src/server/ws_server.c
)

set(CLIENT_SOURCES
    src/client/client_commands.c
    src/client/command_line.c
    src/client/market_monitor.c
    src/client/order_entry.c
    src/client/trade_history.c
    src/client/ws_client.c
)

add_library(quant_trading_lib
    STATIC
    ${TRADING_ENGINE_SOURCES}
    ${UTILS_SOURCES}
    ${PROTOCOL_SOURCES}
    ${CLIENT_SOURCES}
    ${SERVER_SOURCES}
)

target_include_directories(quant_trading_lib
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(quant_trading_lib
    PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CMAKE_THREAD_LIBS_INIT}
    cjson
    websockets
)

target_compile_definitions(quant_trading_lib
    PRIVATE
    $<$<C_COMPILER_ID:GNU>:COMPILER_GCC>
    $<$<C_COMPILER_ID:Clang>:COMPILER_CLANG>
)

# Server executable
add_executable(market_server src/server/server_app.c)
target_link_libraries(market_server
   PRIVATE
   quant_trading_lib
)

# Client executable
add_executable(market_client src/client/client_app.c)
target_link_libraries(market_client
   PRIVATE
   quant_trading_lib
   readline
)

# Install targets
install(TARGETS market_server market_client
        RUNTIME DESTINATION bin)

if(BUILD_TESTS AND UNITY_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()
