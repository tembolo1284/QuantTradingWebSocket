cmake_minimum_required(VERSION 3.10)

# Add an option to use Clang
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

# Set default build type to Debug if not specified
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
option(BUILD_EXAMPLES "Build examples" ON)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)

option(ENABLE_CJSON_TEST "Enable cJSON tests" OFF)
option(ENABLE_CJSON_UTILS "Enable cJSON utils" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

add_subdirectory(third_party/cJSON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

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

set(NET_SOURCES
    src/net/websocket.c
    src/net/websocket_io.c
    src/net/websocket_server.c
    src/net/websocket_frame.c
    src/net/frame.c
    src/net/handshake.c
    src/net/socket.c
    src/net/buffer.c
)

set(TRADING_ENGINE_SOURCES
    src/trading/engine/order.c
    src/trading/engine/order_book.c
    src/trading/engine/matcher.c
)

set(TRADING_PROTOCOL_SOURCES
    src/trading/protocol/messages.c
)

set(UTILS_SOURCES
    src/utils/logging.c
    src/utils/json_utils.c
)

set(COMMON_SOURCES
    src/common/types.c
)

add_library(quant_trading_lib
    STATIC
    ${NET_SOURCES}
    ${TRADING_ENGINE_SOURCES}
    ${TRADING_PROTOCOL_SOURCES}
    ${UTILS_SOURCES}
    ${COMMON_SOURCES}
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
)

target_compile_definitions(quant_trading_lib
    PRIVATE
    $<$<C_COMPILER_ID:GNU>:COMPILER_GCC>
    $<$<C_COMPILER_ID:Clang>:COMPILER_CLANG>
)

add_executable(quant_trading src/server/server_app.c)
target_link_libraries(quant_trading
    PRIVATE
    quant_trading_lib
)

add_executable(market_client src/client/client_app.c)
target_link_libraries(market_client
    PRIVATE
    quant_trading_lib
)

if(BUILD_TESTS AND UNITY_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()
