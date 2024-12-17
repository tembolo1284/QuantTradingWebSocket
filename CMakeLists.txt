cmake_minimum_required(VERSION 3.10)
project(quant_trading C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -march=native")

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Unity setup
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/Unity/src/unity.c)
        set(UNITY_ROOT ${CMAKE_SOURCE_DIR}/third_party/Unity)
        add_library(unity STATIC
            ${UNITY_ROOT}/src/unity.c
        )
        target_include_directories(unity PUBLIC
            ${UNITY_ROOT}/src
        )
        set(UNITY_FOUND TRUE)
    else()
        message(WARNING "Unity not found in third_party/Unity - tests will be disabled")
        set(BUILD_TESTS OFF)
    endif()
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
