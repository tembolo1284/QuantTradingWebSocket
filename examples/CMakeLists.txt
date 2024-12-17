# Market maker example
add_executable(market_maker
    market_maker/market_maker.c
)

target_link_libraries(market_maker
    PRIVATE
    quant_trading_lib
)

# Order book viewer example
add_executable(order_book_viewer
    order_book_viewer/viewer.c
)

target_link_libraries(order_book_viewer
    PRIVATE
    quant_trading_lib
)

# Add any additional examples here
