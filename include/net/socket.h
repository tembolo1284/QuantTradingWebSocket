#ifndef QUANT_TRADING_SOCKET_H
#define QUANT_TRADING_SOCKET_H

#include <stdint.h>
#include <stdbool.h>

// Socket options structure
typedef struct {
    bool non_blocking;
    bool tcp_nodelay;
    bool keep_alive;
    uint32_t connect_timeout_ms;
} SocketOptions;

// Socket connection result
typedef struct {
    int fd;
    int error_code;
    const char* error_message;
} SocketResult;

// Initialize default socket options
void socket_init_options(SocketOptions* options);

// Create and connect a socket
SocketResult socket_create_and_connect(const char* host, uint16_t port, 
                                     const SocketOptions* options);

// Set socket to non-blocking mode
bool socket_set_nonblocking(int sock_fd);

// Configure socket options (TCP_NODELAY, SO_KEEPALIVE, etc.)
bool socket_configure(int sock_fd, const SocketOptions* options);

// Wait for socket to become writable (for non-blocking connects)
bool socket_wait_writable(int sock_fd, uint32_t timeout_ms);

// Get detailed socket state for debugging
char* socket_get_state_string(int sock_fd);

// Clean up socket
void socket_close(int sock_fd);

#endif // QUANT_TRADING_SOCKET_H
