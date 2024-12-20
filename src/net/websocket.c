#include "net/websocket.h"
#include "net/socket.h"
#include "net/handshake.h"
#include "net/frame.h"
#include "net/buffer.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MAX_FRAME_SIZE (16 * 1024 * 1024)  // 16MB max frame size
#define READ_BUFFER_SIZE 8192               // 8KB read buffer

struct WebSocket {
    int sock_fd;
    Buffer* recv_buffer;
    Buffer* send_buffer;
    WebSocketCallbacks callbacks;
    bool connected;
    char* host;     // For logging and reconnection
    uint16_t port;
    uint64_t message_count;   // Statistics
    uint64_t bytes_sent;
    uint64_t bytes_received;
    time_t last_message_time; // For timeout detection
};

static void ws_handle_error(WebSocket* ws, ErrorCode error) {
    LOG_ERROR("WebSocket error occurred: %s", ws_error_string(error));
    if (ws->callbacks.on_error) {
        ws->callbacks.on_error(error, ws->callbacks.user_data);
    }
}

const char* ws_error_string(ErrorCode error) {
    switch (error) {
        case ERROR_NONE:                  return "No error";
        case ERROR_WS_CONNECTION_FAILED:  return "Connection failed";
        case ERROR_WS_HANDSHAKE_FAILED:   return "Handshake failed";
        case ERROR_WS_INVALID_FRAME:      return "Invalid frame";
        case ERROR_WS_SEND_FAILED:        return "Send failed";
        case ERROR_MEMORY:                return "Memory allocation failed";
        case ERROR_TIMEOUT:               return "Operation timed out";
        case ERROR_NETWORK:               return "Network error";
        default:                          return "Unknown error";
    }
}

WebSocket* ws_create(const char* host, uint16_t port, const WebSocketCallbacks* callbacks) {
    LOG_INFO("Creating WebSocket connection to %s:%u", host, port);

    WebSocket* ws = calloc(1, sizeof(WebSocket));
    if (!ws) {
        LOG_ERROR("Failed to allocate WebSocket structure");
        return NULL;
    }

    // Initialize timestamp
    ws->last_message_time = time(NULL);

    // Store connection info for logging and potential reconnection
    ws->host = strdup(host);
    ws->port = port;

    // Initialize socket
    SocketOptions sock_opts;
    socket_init_options(&sock_opts);

    LOG_DEBUG("Creating socket connection");
    SocketResult sock_result = socket_create_and_connect(host, port, &sock_opts);
    if (sock_result.fd < 0) {
        LOG_ERROR("Socket connection failed: %s", sock_result.error_message);
        free(ws->host);
        free(ws);
        return NULL;
    }
    ws->sock_fd = sock_result.fd;

    // Create buffers with larger initial size
    ws->recv_buffer = buffer_create(READ_BUFFER_SIZE);
    ws->send_buffer = buffer_create(READ_BUFFER_SIZE);
    if (!ws->recv_buffer || !ws->send_buffer) {
        LOG_ERROR("Failed to create message buffers");
        if (ws->recv_buffer) buffer_destroy(ws->recv_buffer);
        if (ws->send_buffer) buffer_destroy(ws->send_buffer);
        close(ws->sock_fd);
        free(ws->host);
        free(ws);
        return NULL;
    }

    // Store callbacks
    if (callbacks) {
        ws->callbacks = *callbacks;
    }

    // Perform WebSocket handshake
    HandshakeConfig handshake_cfg;
    handshake_init_config(&handshake_cfg);
    handshake_cfg.host = host;
    handshake_cfg.port = port;

    LOG_DEBUG("Performing WebSocket handshake");
    HandshakeResult handshake = handshake_perform(ws->sock_fd, &handshake_cfg);
    if (!handshake.success) {
        LOG_ERROR("Handshake failed: %s", handshake.error_message);
        buffer_destroy(ws->recv_buffer);
        buffer_destroy(ws->send_buffer);
        close(ws->sock_fd);
        free(ws->host);
        free(ws);
        return NULL;
    }

    handshake_cleanup_result(&handshake);

    ws->connected = true;
    LOG_INFO("WebSocket connection established successfully");
    return ws;
}

bool ws_send(WebSocket* ws, const uint8_t* data, size_t len) {
    if (!ws || !ws->connected || !data) {
        LOG_ERROR("Invalid parameters for ws_send");
        return false;
    }

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

    if (sent < 0) {
        LOG_ERROR("Failed to send frame: %s", strerror(errno));
        free(encoded);
        return false;
    }

    if ((size_t)sent != frame_size) {
        LOG_ERROR("Incomplete send: %zd of %zu bytes", sent, frame_size);
        free(encoded);
        return false;
    }

    free(encoded);
    ws->bytes_sent += sent;
    ws->message_count++;
    ws->last_message_time = time(NULL);

    LOG_DEBUG("Frame sent successfully (total: messages=%lu, bytes=%lu)",
             (unsigned long)ws->message_count, (unsigned long)ws->bytes_sent);
    return true;
}

static void ws_handle_frame(WebSocket* ws, const WebSocketFrame* frame) {
    LOG_DEBUG("Handling frame type=%s, length=%lu",
             frame_type_string(frame->header.opcode),
             (unsigned long)frame->header.payload_len);

    ws->last_message_time = time(NULL);

    switch (frame->header.opcode) {
        case FRAME_TEXT:
        case FRAME_BINARY:
            if (ws->callbacks.on_message) {
                LOG_DEBUG("Delivering message to callback");
                ws->callbacks.on_message(frame->payload, frame->header.payload_len,
                                      ws->callbacks.user_data);
            }
            break;

        case FRAME_PING:
            LOG_DEBUG("Received ping, sending pong");
            {
                WebSocketFrame* pong = frame_create(frame->payload,
                                                 frame->header.payload_len,
                                                 FRAME_PONG);
                if (pong) {
                    ws_send(ws, pong->payload, pong->header.payload_len);
                    frame_destroy(pong);
                }
            }
            break;

        case FRAME_CLOSE:
            LOG_INFO("Received close frame");
            ws->connected = false;
            break;

        case FRAME_PONG:
            LOG_DEBUG("Received pong frame");
            break;

        default:
            LOG_WARN("Unhandled frame type: %s",
                    frame_type_string(frame->header.opcode));
            break;
    }
}

void ws_process(WebSocket* ws) {
    if (!ws || !ws->connected) return;

    // Function to fully read required bytes
    ssize_t read_fully(uint8_t* buffer, size_t needed) {
        size_t total_read = 0;
        size_t attempts = 0;
        const size_t MAX_ATTEMPTS = 10;

        while (total_read < needed && attempts < MAX_ATTEMPTS) {
            ssize_t bytes = read(ws->sock_fd, buffer + total_read, needed - total_read);
            
            if (bytes > 0) {
                total_read += bytes;
                attempts = 0;  // Reset attempts on successful read
                ws->bytes_received += bytes;
            } else if (bytes == 0 || (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                LOG_ERROR("Socket read error or connection closed: %s", strerror(errno));
                return -1;
            } else {
                attempts++;
                usleep(1000);  // Short delay before retry
            }
        }
        
        return total_read;
    }

    while (ws->connected) {
        // Read initial frame header (2 bytes)
        uint8_t header[2];
        ssize_t header_read = read_fully(header, 2);
        if (header_read != 2) {
            if (header_read < 0) {
                ws_handle_error(ws, ERROR_NETWORK);
                return;
            }
            break;  // No more data available
        }

        // Parse basic header
        bool fin = (header[0] & 0x80) != 0;
        uint8_t opcode = header[0] & 0x0F;
        bool masked = (header[1] & 0x80) != 0;
        size_t payload_len = header[1] & 0x7F;
        size_t header_extra = 0;
        uint8_t header_ext[8];

        // Handle extended payload length
        if (payload_len == 126) {
            header_extra = 2;
        } else if (payload_len == 127) {
            header_extra = 8;
        }

        // Read extended header if needed
        if (header_extra > 0) {
            if (read_fully(header_ext, header_extra) != header_extra) {
                ws_handle_error(ws, ERROR_NETWORK);
                return;
            }

            if (payload_len == 126) {
                payload_len = (header_ext[0] << 8) | header_ext[1];
            } else {
                payload_len = 0;  // Initialize to 0 for 64-bit length
                for (int i = 0; i < 8; i++) {
                    payload_len = (payload_len << 8) | header_ext[i];
                }
            }
        }

        // Read masking key if present
        uint8_t mask_key[4];
        if (masked) {
            if (read_fully(mask_key, 4) != 4) {
                ws_handle_error(ws, ERROR_NETWORK);
                return;
            }
        }

        // Validate payload length
        if (payload_len > MAX_FRAME_SIZE) {
            LOG_ERROR("Frame too large: %zu bytes", payload_len);
            ws_handle_error(ws, ERROR_WS_INVALID_FRAME);
            return;
        }

        // Read payload
        uint8_t* payload = NULL;
        if (payload_len > 0) {
            payload = malloc(payload_len);
            if (!payload) {
                LOG_ERROR("Failed to allocate memory for payload");
                ws_handle_error(ws, ERROR_MEMORY);
                return;
            }

            if (read_fully(payload, payload_len) != payload_len) {
                free(payload);
                ws_handle_error(ws, ERROR_NETWORK);
                return;
            }

            // Unmask data if needed
            if (masked) {
                for (size_t i = 0; i < payload_len; i++) {
                    payload[i] ^= mask_key[i % 4];
                }
            }
        }

        // Create frame structure
        WebSocketFrame frame = {
            .header = {
                .fin = fin,
                .opcode = opcode,
                .mask = masked,
                .payload_len = payload_len
            },
            .payload = payload
        };

        // Handle the frame
        if (frame_validate(&frame)) {
            ws_handle_frame(ws, &frame);
        } else {
            LOG_ERROR("Invalid frame received");
            free(payload);
            ws_handle_error(ws, ERROR_WS_INVALID_FRAME);
            return;
        }

        // Cleanup
        free(payload);

        // For control frames, process immediately and return
        if (opcode >= 0x8) {
            break;
        }
    }
}

void ws_close(WebSocket* ws) {
    if (!ws) return;

    LOG_INFO("Closing WebSocket connection (messages=%lu, bytes_sent=%lu, bytes_received=%lu)",
             (unsigned long)ws->message_count,
             (unsigned long)ws->bytes_sent,
             (unsigned long)ws->bytes_received);

    if (ws->connected) {
        LOG_DEBUG("Sending close frame");
        WebSocketFrame* close_frame = frame_create(NULL, 0, FRAME_CLOSE);
        if (close_frame) {
            ws_send(ws, close_frame->payload, close_frame->header.payload_len);
            frame_destroy(close_frame);
        }
    }

    LOG_DEBUG("Closing socket");
    if (ws->sock_fd >= 0) {
        close(ws->sock_fd);
    }

    LOG_DEBUG("Cleaning up resources");
    buffer_destroy(ws->recv_buffer);
    buffer_destroy(ws->send_buffer);
    free(ws->host);
    free(ws);
}

bool ws_is_connected(const WebSocket* ws) {
    return ws && ws->connected;

ssize_t ws_read_fully(WebSocket* ws, uint8_t* buffer, size_t needed) {
    size_t total_read = 0;
    size_t attempts = 0;
    const size_t MAX_ATTEMPTS = 10;

    while (total_read < needed && attempts < MAX_ATTEMPTS) {
        ssize_t bytes = read(ws->sock_fd, buffer + total_read, needed - total_read);
        
        if (bytes > 0) {
            total_read += bytes;
            attempts = 0;  // Reset attempts on successful read
        } else if (bytes == 0 || (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            return -1;  // Error or connection closed
        } else {
            attempts++;
            usleep(1000);  // Short delay before retry
        }
    }
    
    return total_read;
}}
