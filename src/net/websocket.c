#include "net/websocket.h"
#include "net/frame.h"
#include "net/buffer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#define SOCKET_BUFFER_SIZE 65536

struct WebSocket {
    int sock_fd;
    Buffer* recv_buffer;
    Buffer* send_buffer;
    WebSocketCallbacks callbacks;
    bool connected;
};

static char* generate_sec_websocket_key(void) {
    unsigned char random[16];
    for (int i = 0; i < 16; i++) {
        random[i] = rand() & 0xFF;
    }
    
    BIO *bmem = BIO_new(BIO_s_mem());
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_push(b64, bmem);
    BIO_write(b64, random, 16);
    BIO_flush(b64);
    
    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);
    
    char* key = malloc(bptr->length);
    memcpy(key, bptr->data, bptr->length - 1);  // Skip newline
    key[bptr->length - 1] = '\0';
    
    BIO_free_all(b64);
    return key;
}

static bool perform_handshake(WebSocket* ws, const char* host, uint16_t port) {
    char* sec_key = generate_sec_websocket_key();
    if (!sec_key) return false;
    
    // Format handshake request
    char request[1024];
    snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        host, port, sec_key);
    
    free(sec_key);
    
    // Send handshake
    ssize_t sent = send(ws->sock_fd, request, strlen(request), 0);
    if (sent != strlen(request)) return false;
    
    // Receive response
    char response[1024];
    ssize_t received = recv(ws->sock_fd, response, sizeof(response) - 1, 0);
    if (received <= 0) return false;
    
    response[received] = '\0';
    
    // Verify response (simplified)
    return strstr(response, "HTTP/1.1 101") != NULL;
}

WebSocket* ws_create(const char* host, uint16_t port, const WebSocketCallbacks* callbacks) {
    WebSocket* ws = calloc(1, sizeof(WebSocket));
    if (!ws) return NULL;
    
    // Create socket
    ws->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ws->sock_fd < 0) {
        free(ws);
        return NULL;
    }
    
    // Set non-blocking
    fcntl(ws->sock_fd, F_SETFL, O_NONBLOCK);
    
    // Set TCP_NODELAY
    int flag = 1;
    setsockopt(ws->sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    // Connect
    struct hostent* server = gethostbyname(host);
    if (!server) {
        close(ws->sock_fd);
        free(ws);
        return NULL;
    }
    
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (connect(ws->sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        if (errno != EINPROGRESS) {
            close(ws->sock_fd);
            free(ws);
            return NULL;
        }
    }
    
    // Create buffers
    ws->recv_buffer = buffer_create(SOCKET_BUFFER_SIZE);
    ws->send_buffer = buffer_create(SOCKET_BUFFER_SIZE);
    if (!ws->recv_buffer || !ws->send_buffer) {
        ws_close(ws);
        return NULL;
    }
    
    // Set callbacks
    if (callbacks) {
        ws->callbacks = *callbacks;
    }
    
    // Perform WebSocket handshake
    if (!perform_handshake(ws, host, port)) {
        ws_close(ws);
        return NULL;
    }
    
    ws->connected = true;
    return ws;
}

bool ws_send(WebSocket* ws, const uint8_t* data, size_t len) {
    if (!ws || !ws->connected || !data) return false;
    
    WebSocketFrame frame = {0};
    if (frame_create(&frame, data, len, FRAME_BINARY) < 0) {
        return false;
    }
    
    // Write frame to send buffer
    bool success = buffer_write(ws->send_buffer, frame.payload, frame.payload_length);
    free(frame.payload);
    
    return success;
}

void ws_process(WebSocket* ws) {
    if (!ws || !ws->connected) return;
    
    // Read available data
    uint8_t temp_buffer[4096];
    ssize_t bytes;
    
    while ((bytes = recv(ws->sock_fd, temp_buffer, sizeof(temp_buffer), 0)) > 0) {
        buffer_write(ws->recv_buffer, temp_buffer, bytes);
    }
    
    // Process complete frames
    while (ws->recv_buffer->size > 0) {
        WebSocketFrame frame = {0};
        int parsed = frame_parse(ws->recv_buffer->data + ws->recv_buffer->read_pos,
                               ws->recv_buffer->size - ws->recv_buffer->read_pos,
                               &frame);
        
        if (parsed < 0) break;  // Incomplete frame
        
        ws->recv_buffer->read_pos += parsed;
        
        // Handle frame
        switch (frame.type) {
            case FRAME_BINARY:
            case FRAME_TEXT:
                if (ws->callbacks.on_message) {
                    ws->callbacks.on_message(frame.payload, frame.payload_length, 
                                          ws->callbacks.user_data);
                }
                break;
                
            case FRAME_PING:
                // Send pong
                frame_create(&frame, NULL, 0, FRAME_PONG);
                ws_send(ws, frame.payload, frame.payload_length);
                break;
                
            case FRAME_CLOSE:
                ws->connected = false;
                break;
        }
        
        free(frame.payload);
    }
    
    // Send buffered data
    if (ws->send_buffer->size > 0) {
        ssize_t sent = send(ws->sock_fd, 
                          ws->send_buffer->data + ws->send_buffer->read_pos,
                          ws->send_buffer->size - ws->send_buffer->read_pos,
                          0);
        
        if (sent > 0) {
            ws->send_buffer->read_pos += sent;
        }
    }
}

void ws_close(WebSocket* ws) {
    if (!ws) return;
    
    if (ws->connected) {
        // Send close frame
        WebSocketFrame frame = {0};
        frame_create(&frame, NULL, 0, FRAME_CLOSE);
        ws_send(ws, frame.payload, frame.payload_length);
        free(frame.payload);
    }
    
    if (ws->sock_fd >= 0) {
        close(ws->sock_fd);
    }
    
    buffer_destroy(ws->recv_buffer);
    buffer_destroy(ws->send_buffer);
    free(ws);
}
