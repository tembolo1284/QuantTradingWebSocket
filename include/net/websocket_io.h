#ifndef WEBSOCKET_IO_H
#define WEBSOCKET_IO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <signal.h>
#include "common/types.h"
#include "net/websocket.h"
#include "net/buffer.h"

// I/O related constants
#define WS_MAX_FRAME_SIZE (16 * 1024 * 1024)  // 16MB max frame size
#define WS_READ_BUFFER_SIZE 8192               // 8KB read buffer

// Shutdown state management
void ws_io_init_signal_handlers(void);
bool ws_io_shutdown_requested(void);
bool ws_io_force_shutdown(void);

// Socket operations
int ws_io_setup_socket(const char* host, uint16_t port);
void ws_io_set_nonblocking(int sock_fd);

// Buffer management
Buffer* ws_io_create_buffer(void);
void ws_io_cleanup_buffers(WebSocket* ws);

// Core I/O operations
ssize_t ws_io_read_fully(WebSocket* ws, uint8_t* buffer, size_t needed);
bool ws_io_send_frame(WebSocket* ws, const uint8_t* data, size_t len);

// Cleanup
void ws_io_cleanup_socket(WebSocket* ws);

#endif // WEBSOCKET_IO_H
