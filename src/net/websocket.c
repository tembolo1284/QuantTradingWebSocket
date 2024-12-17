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

struct WebSocket {
    int sock_fd;
    Buffer* recv_buffer;
    Buffer* send_buffer;
    WebSocketCallbacks callbacks;
    bool connected;
    char* host;  // For logging and reconnection
    uint16_t port;
    uint64_t message_count;  // Statistics
    uint64_t bytes_sent;
    uint64_t bytes_received;
    time_t last_buffer_reset_log;  // Track log frequency for buffer resets
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

    // Initialize last buffer reset log timestamp
    ws->last_buffer_reset_log = time(NULL);

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

    // Create buffers
    ws->recv_buffer = buffer_create(65536);  // 64KB initial size
    ws->send_buffer = buffer_create(65536);
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

    LOG_DEBUG("Frame sent successfully (total: messages=%lu, bytes=%lu)",
             (unsigned long)ws->message_count, (unsigned long)ws->bytes_sent);
    return true;
}

static void ws_handle_frame(WebSocket* ws, const WebSocketFrame* frame) {
    LOG_DEBUG("Handling frame type=%s, length=%lu",
             frame_type_string(frame->header.opcode),
             (unsigned long)frame->header.payload_len);

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

    // Read available data
    uint8_t temp_buffer[4096];
    ssize_t bytes;

    while ((bytes = read(ws->sock_fd, temp_buffer, sizeof(temp_buffer))) > 0) {
        LOG_DEBUG("Read %zd bytes from socket", bytes);

        if (!buffer_write(ws->recv_buffer, temp_buffer, bytes)) {
            LOG_ERROR("Failed to write to receive buffer");
            ws_handle_error(ws, ERROR_MEMORY);
            return;
        }

        ws->bytes_received += bytes;
    }

    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        LOG_ERROR("Socket read error: %s", strerror(errno));
        ws_handle_error(ws, ERROR_WS_CONNECTION_FAILED);
        return;
    }

    // Process complete frames
    while (ws->connected && ws->recv_buffer->size > 0) {
        WebSocketFrame* frame = NULL;
        FrameParseResult result = frame_parse(
            ws->recv_buffer->data + ws->recv_buffer->read_pos,
            ws->recv_buffer->size - ws->recv_buffer->read_pos,
            &frame
        );

        if (!result.complete) {
            if (result.error) {
                LOG_ERROR("Frame parsing failed: %s", result.error);
                free(result.error);
                ws_handle_error(ws, ERROR_WS_INVALID_FRAME);
            }
            break;
        }

        ws->recv_buffer->read_pos += result.bytes_consumed;

        if (frame) {
            if (frame_validate(frame)) {
                ws_handle_frame(ws, frame);
            } else {
                LOG_ERROR("Invalid frame received");
                ws_handle_error(ws, ERROR_WS_INVALID_FRAME);
            }
            frame_destroy(frame);
        }
    }

    // Buffer maintenance
    if (ws->recv_buffer->read_pos == ws->recv_buffer->size) {
        // Only log buffer reset every 30 seconds to reduce log spam
        time_t now = time(NULL);
        if (now - ws->last_buffer_reset_log >= 30) {
            LOG_DEBUG("Resetting receive buffer positions");
            ws->last_buffer_reset_log = now;
        }
        
        ws->recv_buffer->read_pos = 0;
        ws->recv_buffer->size = 0;
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

    if (ws->sock_fd >= 0) {
        LOG_DEBUG("Closing socket");
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
}
