#include "net/websocket.h"
#include "net/websocket_io.h"
#include "common/types.h"
#include "net/handshake.h"
#include "net/frame.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char* ws_error_string(ErrorCode error) {
    return error_code_to_string(error);
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

WebSocket* ws_create(const char* host, uint16_t port, const WebSocketCallbacks* callbacks) {
    LOG_INFO("Creating WebSocket connection to %s:%u", host, port);
    
    // Initialize signal handlers
    ws_io_init_signal_handlers();

    WebSocket* ws = calloc(1, sizeof(WebSocket));
    if (!ws) {
        LOG_ERROR("Failed to allocate WebSocket structure");
        return NULL;
    }

    // Initialize timestamp and connection info
    ws->last_message_time = time(NULL);
    ws->host = strdup(host);
    ws->port = port;

    // Set up socket
    ws->sock_fd = ws_io_setup_socket(host, port);
    if (ws->sock_fd < 0) {
        LOG_ERROR("Socket setup failed");
        free(ws->host);
        free(ws);
        return NULL;
    }

    // Set socket to non-blocking mode
    ws_io_set_nonblocking(ws->sock_fd);

    // Create buffers
    ws->recv_buffer = ws_io_create_buffer();
    ws->send_buffer = ws_io_create_buffer();
    if (!ws->recv_buffer || !ws->send_buffer) {
        LOG_ERROR("Failed to create message buffers");
        ws_io_cleanup_buffers(ws);
        ws_io_cleanup_socket(ws);
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
        ws_io_cleanup_buffers(ws);
        ws_io_cleanup_socket(ws);
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
    
    return ws_io_send_frame(ws, data, len);
}

void ws_process(WebSocket* ws) {
    if (!ws || !ws->connected) {
        return;
    }

    if (ws_io_shutdown_requested() || ws_io_force_shutdown()) {
        LOG_INFO("Shutdown requested, cleaning up...");
        ws_close(ws);
        return;
    }

    // Check connection health
    if (time(NULL) - ws->last_message_time > 30) {  // 30 second timeout
        LOG_WARN("No messages received for 30 seconds, checking connection");
        WebSocketFrame* ping = frame_create(NULL, 0, FRAME_PING);
        if (ping) {
            ws_send(ws, ping->payload, ping->header.payload_len);
            frame_destroy(ping);
        }
    }

    // First try to read the initial 2-byte header
    uint8_t header[2];
    ssize_t header_read = ws_io_read_fully(ws, header, 2);
    
    if (header_read < 0) {
        if (ws_io_shutdown_requested()) {
            LOG_INFO("Shutdown detected during header read");
        } else {
            LOG_ERROR("Fatal error reading header, closing connection");
            ws->connected = false;
        }
        return;
    }
    
    if (header_read != 2) {
        if (header_read != 0) {  // 0 means no data available, which is normal
            LOG_ERROR("Incomplete header read (%zd bytes), attempting recovery", header_read);
            struct timespec ts = {0, 100000000}; // 100ms
            nanosleep(&ts, NULL);
        }
        return;
    }

    // Parse frame header
    bool fin = (header[0] & 0x80) != 0;
    uint8_t opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;

    // Handle extended payload length
    uint8_t ext_len[8];
    if (payload_len == 126) {
        if (ws_io_read_fully(ws, ext_len, 2) != 2) {
            LOG_ERROR("Failed to read extended length (16-bit)");
            ws->connected = false;
            return;
        }
        payload_len = (ext_len[0] << 8) | ext_len[1];
    } else if (payload_len == 127) {
        if (ws_io_read_fully(ws, ext_len, 8) != 8) {
            LOG_ERROR("Failed to read extended length (64-bit)");
            ws->connected = false;
            return;
        }
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext_len[i];
        }
    }

    // Validate payload length
    if (payload_len > WS_MAX_FRAME_SIZE) {
        LOG_ERROR("Frame too large: %lu bytes", (unsigned long)payload_len);
        ws->connected = false;
        return;
    }

    // Read masking key if present
    uint8_t mask_key[4];
    if (masked) {
        if (ws_io_read_fully(ws, mask_key, 4) != 4) {
            LOG_ERROR("Failed to read mask key");
            ws->connected = false;
            return;
        }
    }

    // Read payload
    uint8_t* payload = NULL;
    if (payload_len > 0) {
        payload = malloc(payload_len);
        if (!payload) {
            LOG_ERROR("Failed to allocate memory for payload");
            ws->connected = false;
            return;
        }

        ssize_t payload_read = ws_io_read_fully(ws, payload, payload_len);
        if (payload_read != (ssize_t)payload_len) {
            LOG_ERROR("Incomplete payload read: %zd of %lu bytes",
                     payload_read, (unsigned long)payload_len);
            free(payload);
            ws->connected = false;
            return;
        }

        // Unmask if needed
        if (masked) {
            for (size_t i = 0; i < payload_len; i++) {
                payload[i] ^= mask_key[i % 4];
            }
        }
    }

    // Create and process frame
    WebSocketFrame frame = {
        .header = {
            .fin = fin,
            .opcode = opcode,
            .mask = masked,
            .payload_len = payload_len
        },
        .payload = payload
    };

    if (frame_validate(&frame)) {
        ws_handle_frame(ws, &frame);
    } else {
        LOG_ERROR("Invalid frame received");
        ws->connected = false;
    }

    free(payload);

    // For control frames, return immediately after processing
    if (opcode >= 0x8) {
        return;
    }
}

void ws_close(WebSocket* ws) {
    if (!ws) return;

    LOG_INFO("Closing WebSocket connection...");
    
    if (ws->connected) {
        // Send close frame
        uint8_t close_payload[] = {0x03, 0xE8}; // 1000 in network byte order
        WebSocketFrame* close_frame = frame_create(close_payload, 2, FRAME_CLOSE);
        if (close_frame) {
            LOG_DEBUG("Sending close frame");
            ws_send(ws, close_frame->payload, close_frame->header.payload_len);
            frame_destroy(close_frame);
        }
    }

    // Clean up resources
    ws_io_cleanup_buffers(ws);
    ws_io_cleanup_socket(ws);
    
    if (ws->host) {
        free(ws->host);
        ws->host = NULL;
    }
    
    free(ws);
    LOG_INFO("WebSocket cleanup complete");
}

bool ws_is_connected(const WebSocket* ws) {
    return ws && ws->connected;
}
