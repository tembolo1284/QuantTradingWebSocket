# Collect source files
set(SOURCES
    net/websocket.c
    net/frame.c
    net/buffer.c
    trading/order.c
    trading/order_book.c
    utils/heap.c
)

# Create library
add_library(quant_trading_lib STATIC ${SOURCES})

target_link_libraries(quant_trading_lib
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CMAKE_THREAD_LIBS_INIT}
)

# Create executable
add_executable(quant_trading main.c)

target_link_libraries(quant_trading
    PRIVATE
    quant_trading_lib
)
