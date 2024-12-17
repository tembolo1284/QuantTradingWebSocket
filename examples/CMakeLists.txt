# Check for market maker source file
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simple_market_maker/market_maker.c)
    add_executable(market_maker
        simple_market_maker/market_maker.c
    )
    target_link_libraries(market_maker
        PRIVATE
        quant_trading_lib
    )
    message(STATUS "Found market maker example")
else()
    message(STATUS "Market maker source not found at ${CMAKE_CURRENT_SOURCE_DIR}/simple_market_maker/market_maker.c")
endif()

# Check for order book viewer source file
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/order_book_viewer/viewer.c)
    add_executable(order_book_viewer
        order_book_viewer/viewer.c
    )
    target_link_libraries(order_book_viewer
        PRIVATE
        quant_trading_lib
    )
    message(STATUS "Found order book viewer example")
else()
    message(STATUS "Order book viewer source not found at ${CMAKE_CURRENT_SOURCE_DIR}/order_book_viewer/viewer.c")
endif()

# Add placeholder target if no examples are found
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simple_market_maker/market_maker.c AND
   NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/order_book_viewer/viewer.c)
    message(WARNING "No example source files found. Please ensure the following files exist:")
    message(WARNING "  - ${CMAKE_CURRENT_SOURCE_DIR}/simple_market_maker/market_maker.c")
    message(WARNING "  - ${CMAKE_CURRENT_SOURCE_DIR}/order_book_viewer/viewer.c")
    add_custom_target(examples
        COMMENT "No examples found. Please implement the example files."
    )
endif()

# Include directories for examples
include_directories(${CMAKE_SOURCE_DIR}/include)
