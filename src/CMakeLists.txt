# Network source files
set(NET_SOURCES
    net/websocket.c
    net/frame.c
    net/handshake.c
    net/socket.c
    net/buffer.c
)

# Trading source files
set(TRADING_SOURCES
    trading/order.c
    trading/order_book.c
)

# Utility source files
set(UTILS_SOURCES
    utils/logging.c
)

# Create library
add_library(quant_trading_lib STATIC
    ${NET_SOURCES}
    ${TRADING_SOURCES}
    ${UTILS_SOURCES}
)

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
