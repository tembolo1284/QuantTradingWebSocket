#include "net/socket.h"
#include "utils/logging.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void socket_init_options(SocketOptions* options) {
    LOG_DEBUG("Initializing socket options");
    options->non_blocking = true;
    options->tcp_nodelay = true;
    options->keep_alive = true;
    options->connect_timeout_ms = 5000;  // 5 seconds default
    LOG_DEBUG("Socket options initialized: non_blocking=%d, tcp_nodelay=%d, keep_alive=%d, timeout=%u ms",
             options->non_blocking, options->tcp_nodelay, options->keep_alive, 
             options->connect_timeout_ms);
}

static char* get_socket_error_string(int sock_fd) {
    char* result = malloc(1024);
    if (!result) return NULL;

    // Get socket error if any
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        snprintf(result, 1024, "Failed to get socket error: %s", strerror(errno));
    } else if (error != 0) {
        snprintf(result, 1024, "Socket error: %s", strerror(error));
    } else {
        // Get socket state information
        int opt_val;
        len = sizeof(opt_val);
        
        snprintf(result, 1024, "Socket state: ");
        size_t offset = strlen(result);

        if (getsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &opt_val, &len) == 0) {
            offset += snprintf(result + offset, 1024 - offset, 
                             "KEEPALIVE=%s, ", opt_val ? "on" : "off");
        }
        
        if (getsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &opt_val, &len) == 0) {
            offset += snprintf(result + offset, 1024 - offset, 
                             "TCP_NODELAY=%s, ", opt_val ? "on" : "off");
        }

        // Get socket type
        int sock_type;
        len = sizeof(sock_type);
        if (getsockopt(sock_fd, SOL_SOCKET, SO_TYPE, &sock_type, &len) == 0) {
            offset += snprintf(result + offset, 1024 - offset, 
                             "Type=%s", sock_type == SOCK_STREAM ? "STREAM" : "OTHER");
        }
    }

    return result;
}

SocketResult socket_create_and_connect(const char* host, uint16_t port, 
                                     const SocketOptions* options) {
    LOG_INFO("Creating socket connection to %s:%u", host, port);
    SocketResult result = {-1, 0, NULL};

    // Create socket
    result.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (result.fd < 0) {
        result.error_code = errno;
        result.error_message = strdup(strerror(errno));
        LOG_ERROR("Failed to create socket: %s", result.error_message);
        return result;
    }
    LOG_DEBUG("Created socket fd: %d", result.fd);

    // Configure socket options
    if (options) {
        LOG_DEBUG("Configuring socket options");
        if (!socket_configure(result.fd, options)) {
            result.error_code = errno;
            result.error_message = strdup("Failed to configure socket options");
            LOG_ERROR("%s: %s", result.error_message, strerror(errno));
            close(result.fd);
            result.fd = -1;
            return result;
        }
    }

    // Resolve hostname
    LOG_DEBUG("Resolving hostname: %s", host);
    struct hostent* server = gethostbyname(host);
    if (!server) {
        result.error_code = h_errno;
        result.error_message = strdup(hstrerror(h_errno));
        LOG_ERROR("Failed to resolve hostname: %s", result.error_message);
        close(result.fd);
        result.fd = -1;
        return result;
    }

    // Prepare address structure
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    // Attempt connection
    LOG_DEBUG("Attempting connection to %s:%u", host, port);
    if (connect(result.fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        if (errno != EINPROGRESS || !options || !options->non_blocking) {
            result.error_code = errno;
            result.error_message = strdup(strerror(errno));
            LOG_ERROR("Connection failed: %s", result.error_message);
            close(result.fd);
            result.fd = -1;
            return result;
        }

        LOG_DEBUG("Connection in progress (non-blocking mode)");
        
        // Wait for connection completion
        if (!socket_wait_writable(result.fd, options->connect_timeout_ms)) {
            result.error_code = errno;
            result.error_message = strdup("Connection timeout");
            LOG_ERROR("Connection timeout after %u ms", options->connect_timeout_ms);
            close(result.fd);
            result.fd = -1;
            return result;
        }

        // Verify connection success
        int error;
        socklen_t error_len = sizeof(error);
        if (getsockopt(result.fd, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0) {
            result.error_code = errno;
            result.error_message = strdup("Failed to get socket error state");
            LOG_ERROR("%s: %s", result.error_message, strerror(errno));
            close(result.fd);
            result.fd = -1;
            return result;
        }

        if (error != 0) {
            result.error_code = error;
            result.error_message = strdup(strerror(error));
            LOG_ERROR("Connection failed after wait: %s", result.error_message);
            close(result.fd);
            result.fd = -1;
            return result;
        }
    }

    LOG_INFO("Successfully connected to %s:%u", host, port);
    char* socket_state = get_socket_error_string(result.fd);
    if (socket_state) {
        LOG_DEBUG("Socket state: %s", socket_state);
        free(socket_state);
    }

    return result;
}

bool socket_set_nonblocking(int sock_fd) {
    LOG_DEBUG("Setting socket %d to non-blocking mode", sock_fd);
    
    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags < 0) {
        LOG_ERROR("Failed to get socket flags: %s", strerror(errno));
        return false;
    }

    if (fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR("Failed to set non-blocking mode: %s", strerror(errno));
        return false;
    }

    LOG_DEBUG("Successfully set socket %d to non-blocking mode", sock_fd);
    return true;
}

bool socket_configure(int sock_fd, const SocketOptions* options) {
    LOG_DEBUG("Configuring socket %d", sock_fd);

    if (options->non_blocking && !socket_set_nonblocking(sock_fd)) {
        return false;
    }

    if (options->tcp_nodelay) {
        LOG_DEBUG("Setting TCP_NODELAY on socket %d", sock_fd);
        int flag = 1;
        if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            LOG_ERROR("Failed to set TCP_NODELAY: %s", strerror(errno));
            return false;
        }
    }

    if (options->keep_alive) {
        LOG_DEBUG("Setting SO_KEEPALIVE on socket %d", sock_fd);
        int flag = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0) {
            LOG_ERROR("Failed to set SO_KEEPALIVE: %s", strerror(errno));
            return false;
        }
    }

    LOG_DEBUG("Successfully configured socket %d", sock_fd);
    return true;
}

bool socket_wait_writable(int sock_fd, uint32_t timeout_ms) {
    LOG_DEBUG("Waiting for socket %d to become writable (timeout: %u ms)", 
             sock_fd, timeout_ms);

    fd_set write_fds;
    struct timeval tv;
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    FD_ZERO(&write_fds);
    FD_SET(sock_fd, &write_fds);

    int result = select(sock_fd + 1, NULL, &write_fds, NULL, &tv);
    
    if (result < 0) {
        LOG_ERROR("Select failed while waiting for socket: %s", strerror(errno));
        return false;
    }
    
    if (result == 0) {
        LOG_ERROR("Socket wait timed out after %u ms", timeout_ms);
        return false;
    }

    LOG_DEBUG("Socket %d became writable", sock_fd);
    return true;
}

char* socket_get_state_string(int sock_fd) {
    LOG_DEBUG("Getting state string for socket %d", sock_fd);
    return get_socket_error_string(sock_fd);
}

void socket_close(int sock_fd) {
    if (sock_fd >= 0) {
        LOG_DEBUG("Closing socket %d", sock_fd);
        close(sock_fd);
    }
}
