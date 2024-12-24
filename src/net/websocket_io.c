#include "net/websocket_io.h"
#include "net/websocket.h"
#include "net/socket.h"
#include "net/frame.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <time.h>

// Global shutdown flags
static volatile sig_atomic_t shutdown_requested = 0;
static volatile sig_atomic_t force_shutdown = 0;
static volatile sig_atomic_t signal_count = 0;

// Signal handler for graceful shutdown
static void ws_signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        signal_count++;
        if (signal_count == 1) {
            shutdown_requested = 1;
        } else if (signal_count >= 3) {
            force_shutdown = 1;
            exit(1);
        }
    }
}

void ws_io_init_signal_handlers(void) {
    struct sigaction sa = {
        .sa_handler = ws_signal_handler,
        .sa_flags = 0
    };
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_ERROR("Failed to set up signal handlers");
    }
}

bool ws_io_shutdown_requested(void) {
    return shutdown_requested != 0;
}

bool ws_io_force_shutdown(void) {
    return force_shutdown != 0;
}

int ws_io_setup_socket(const char* host, uint16_t port) {
    SocketOptions sock_opts;
    socket_init_options(&sock_opts);

    LOG_DEBUG("Creating socket connection");
    SocketResult sock_result = socket_create_and_connect(host, port, &sock_opts);
    if (sock_result.fd < 0) {
        LOG_ERROR("Socket connection failed: %s", sock_result.error_message);
        return -1;
    }
    
    return sock_result.fd;
}

void ws_io_set_nonblocking(int sock_fd) {
    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags >= 0) {
        flags |= O_NONBLOCK;
        if (fcntl(sock_fd, F_SETFL, flags) < 0) {
            LOG_ERROR("Failed to set socket non-blocking: %s", strerror(errno));
        }
    } else {
        LOG_ERROR("Failed to get socket flags: %s", strerror(errno));
    }
}

Buffer* ws_io_create_buffer(void) {
    return buffer_create(WS_READ_BUFFER_SIZE);
}

void ws_io_cleanup_buffers(WebSocket* ws) {
    if (!ws) return;
    
    if (ws->recv_buffer) {
        buffer_destroy(ws->recv_buffer);
        ws->recv_buffer = NULL;
    }
    
    if (ws->send_buffer) {
        buffer_destroy(ws->send_buffer);
        ws->send_buffer = NULL;
    }
}

ssize_t ws_io_read_fully(WebSocket* ws, uint8_t* buffer, size_t needed) {
    if (!ws || !buffer) return -1;
    
    ssize_t total_read = 0;
    const int READ_TIMEOUT_MS = 50;
    int retry_count = 0;
    const int MAX_RETRIES = 3;
    
    while ((size_t)total_read < needed && !shutdown_requested && retry_count < MAX_RETRIES) {
        fd_set read_fds;
        struct timeval tv = {0, READ_TIMEOUT_MS * 1000};
        
        FD_ZERO(&read_fds);
        FD_SET(ws->sock_fd, &read_fds);
        
        int ready = select(ws->sock_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (ready < 0) {
            if (errno == EINTR) {
                if (shutdown_requested) {
                    LOG_INFO("Read interrupted by shutdown request");
                    return -1;
                }
                continue;
            }
            LOG_ERROR("Select error in read_fully: %s", strerror(errno));
            return -1;
        }
        
        if (ready == 0) {
            LOG_DEBUG("Timeout waiting for data");
            retry_count++;
            continue;
        }
        
        ssize_t bytes = read(ws->sock_fd, buffer + total_read, needed - (size_t)total_read);
        
        if (bytes > 0) {
            total_read += bytes;
            ws->bytes_received += (uint64_t)bytes;
            ws->last_message_time = time(NULL);
            retry_count = 0;
        } else if (bytes == 0) {
            LOG_ERROR("Connection closed by peer");
            return total_read > 0 ? total_read : -1;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                retry_count++;
                continue;
            }
            LOG_ERROR("Read error: %s", strerror(errno));
            return -1;
        }
    }
    
    if (retry_count >= MAX_RETRIES) {
        LOG_ERROR("Max retries reached waiting for data");
        return total_read > 0 ? total_read : -1;
    }
    
    return total_read;
}

bool ws_io_send_frame(WebSocket* ws, const uint8_t* data, size_t len) {
    if (!ws || !data) return false;

    LOG_DEBUG("Creating WebSocket frame for %zu bytes of data", len);
    WebSocketFrame* frame = frame_create(data, len, FRAME_BINARY);
    if (!frame) {
        LOG_ERROR("Failed to create WebSocket frame");
        return false;
    }

    size_t frame_size;
    uint8_t* encoded = frame_encode(frame, &frame_size);
    frame_destroy(frame);

    if (!encoded) {
        LOG_ERROR("Failed to encode WebSocket frame");
        return false;
    }

    LOG_DEBUG("Sending frame of size %zu", frame_size);
    ssize_t sent = write(ws->sock_fd, encoded, frame_size);

    bool success = true;
    if (sent < 0) {
        LOG_ERROR("Failed to send frame: %s", strerror(errno));
        success = false;
    } else if ((size_t)sent != frame_size) {
        LOG_ERROR("Incomplete send: %zd of %zu bytes", sent, frame_size);
        success = false;
    } else {
        ws->bytes_sent += sent;
        ws->message_count++;
        ws->last_message_time = time(NULL);
    }

    free(encoded);
    return success;
}

void ws_io_cleanup_socket(WebSocket* ws) {
    if (!ws) return;
    
    if (ws->sock_fd >= 0) {
        shutdown(ws->sock_fd, SHUT_RDWR);
        close(ws->sock_fd);
        ws->sock_fd = -1;
    }
}
