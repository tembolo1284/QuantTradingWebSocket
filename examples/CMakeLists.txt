# Market Maker example
add_executable(market_maker
    simple_market_maker/market_maker.c
)

target_link_libraries(market_maker
    PRIVATE
    quant_trading_lib
)

# Order Book Viewer example
add_executable(order_book_viewer
    order_book_viewer/viewer.c
)

target_link_libraries(order_book_viewer
    PRIVATE
    quant_trading_lib
)
