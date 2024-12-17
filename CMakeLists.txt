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
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -fsanitize=address")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -march=native")

# Options
option(BUILD_TESTS "Build tests" ON)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Make Criterion optional
if(BUILD_TESTS)
    find_package(Criterion QUIET)
    if(NOT Criterion_FOUND)
        message(WARNING "Criterion not found - tests will be disabled")
        set(BUILD_TESTS OFF)
    endif()
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(src)

if(BUILD_TESTS AND Criterion_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()
