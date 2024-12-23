#define _GNU_SOURCE  // to enable memmem() if available
#include "net/websocket_server.h"
#include "net/socket.h"
#include "net/handshake.h"
#include "net/buffer.h"
#include "net/websocket_frame.h"
#include "utils/logging.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <stdatomic.h>

// #define MAX_CLIENTS 64
#define BUFFER_SIZE 4096
#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
//static atomic_uint_fast32_t next_client_id = ATOMIC_VAR_INIT(1);

/*
// Internal WebSocket server structure
struct WebSocketServer {
    int server_socket;
    uint16_t port;
    WebSocketServerConfig config;
    WebSocketClient* clients[MAX_CLIENTS];
    int client_count;
    volatile bool shutdown_requested;
};

// Internal WebSocket client structure
struct WebSocketClient {
    int socket;
    bool is_websocket;
    bool handshake_complete;
    void* user_data;
    Buffer* read_buffer;
    Buffer* write_buffer;
    uint32_t client_id;
};
*/

// Portable memmem implementation
static void* portable_memmem(const void* haystack, size_t haystacklen, 
                              const void* needle, size_t needlelen) {
    if (needlelen > haystacklen) return NULL;
    
    for (size_t i = 0; i <= haystacklen - needlelen; i++) {
        if (memcmp((char*)haystack + i, needle, needlelen) == 0) {
            return (char*)haystack + i;
        }
    }
    
    return NULL;
}

// Base64 encoding function
static char* base64_encode(const unsigned char* input, size_t length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);

    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char* buff = malloc(bptr->length + 1);
    if (buff) {
        memcpy(buff, bptr->data, bptr->length);
        buff[bptr->length] = 0;
    }

    BIO_free_all(b64);
    return buff;
}

// Perform WebSocket handshake
static bool perform_websocket_handshake(WebSocketClient* client, const uint8_t* data, size_t len) {
    // Find WebSocket key
    char* key_start = portable_memmem((char*)data, len, "Sec-WebSocket-Key: ", strlen("Sec-WebSocket-Key: "));
    if (!key_start) {
        LOG_ERROR("No WebSocket key found in handshake request");
        return false;
    }

    key_start += strlen("Sec-WebSocket-Key: ");
    char key[256] = {0};
    sscanf(key_start, "%255[^\r\n]", key);

    // Generate accept key
    char concat[300];
    snprintf(concat, sizeof(concat), "%s%s", key, WEBSOCKET_GUID);

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)concat, strlen(concat), hash);

    char* accept_key = base64_encode(hash, SHA_DIGEST_LENGTH);
    if (!accept_key) {
        LOG_ERROR("Failed to generate accept key");
        return false;
    }

    // Prepare handshake response
    char response[1024];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_key);

    free(accept_key);

    // Send the handshake response
    ssize_t sent = write(client->socket, response, response_len);
    if (sent != response_len) {
        LOG_ERROR("Failed to send WebSocket handshake response");
        return false;
    }

    client->is_websocket = true;
    client->handshake_complete = true;
    LOG_INFO("WebSocket handshake completed successfully");
    return true;
}

// Create a new WebSocket client
// Add this at the top of the file with other static variables
static atomic_uint_fast32_t next_client_id = ATOMIC_VAR_INIT(1);

static WebSocketClient* create_websocket_client(int socket) {
    // Validate socket
    if (socket < 0) {
        LOG_ERROR("Invalid socket passed to create_websocket_client");
        return NULL;
    }

    // Allocate client structure
    WebSocketClient* client = malloc(sizeof(WebSocketClient));
    if (!client) {
        LOG_ERROR("Memory allocation failed for WebSocket client");
        return NULL;
    }

    // Initialize all fields to zero/default values
    memset(client, 0, sizeof(WebSocketClient));

    // Set socket and generate unique client ID
    client->socket = socket;
    client->client_id = atomic_fetch_add(&next_client_id, 1);

    // Create read buffer
    client->read_buffer = buffer_create(BUFFER_SIZE);
    if (!client->read_buffer) {
        LOG_ERROR("Failed to create read buffer for client %u", client->client_id);
        free(client);
        return NULL;
    }

    // Create write buffer
    client->write_buffer = buffer_create(BUFFER_SIZE);
    if (!client->write_buffer) {
        LOG_ERROR("Failed to create write buffer for client %u", client->client_id);
        buffer_destroy(client->read_buffer);
        free(client);
        return NULL;
    }

    // Set initial connection state
    client->is_websocket = false;
    client->handshake_complete = false;
    client->user_data = NULL;

    // Optional: Set socket to non-blocking mode
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags != -1) {
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    } else {
        LOG_WARN("Failed to set socket to non-blocking mode for client %u", client->client_id);
    }

    LOG_DEBUG("Created WebSocket client (ID: %u, Socket: %d)", 
              client->client_id, client->socket);

    return client;
}

static void destroy_websocket_client(WebSocketClient* client) {
    if (!client) return;

    LOG_DEBUG("Destroying WebSocket client (ID: %u, Socket: %d)", 
              client->client_id, client->socket);

    // Shutdown socket to prevent resource leaks
    if (client->socket > 0) {
        shutdown(client->socket, SHUT_RDWR);
        close(client->socket);
    }

    // Safely destroy buffers
    if (client->read_buffer) {
        buffer_destroy(client->read_buffer);
        client->read_buffer = NULL;
    }

    if (client->write_buffer) {
        buffer_destroy(client->write_buffer);
        client->write_buffer = NULL;
    }

    // Free any user data if needed (depends on your specific implementation)
    // You might want to add a user_data_free callback if complex cleanup is required
    client->user_data = NULL;

    // Finally, free the client structure
    free(client);
}

// Create a new WebSocket server
WebSocketServer* ws_server_create(const WebSocketServerConfig* config) {
    if (!config) {
        LOG_ERROR("Invalid WebSocket server configuration");
        return NULL;
    }

    // Create server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        LOG_ERROR("Failed to create server socket: %s", strerror(errno));
        return NULL;
    }

    // Set socket options to reuse address
    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        LOG_ERROR("Failed to set socket options: %s", strerror(errno));
        close(server_socket);
        return NULL;
    }

    // Bind to address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config->port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to bind server socket: %s", strerror(errno));
        close(server_socket);
        return NULL;
    }

    // Start listening
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        LOG_ERROR("Failed to listen on server socket: %s", strerror(errno));
        close(server_socket);
        return NULL;
    }

    // Allocate server structure
    WebSocketServer* server = malloc(sizeof(WebSocketServer));
    if (!server) {
        LOG_ERROR("Failed to allocate memory for WebSocket server");
        close(server_socket);
        return NULL;
    }

    // Initialize server structure
    server->server_socket = server_socket;
    server->port = config->port;
    server->config = *config;
    server->client_count = 0;
    server->shutdown_requested = false;
    memset(server->clients, 0, sizeof(server->clients));

    LOG_INFO("WebSocket server created on port %u", config->port);
    return server;
}

// Request server shutdown
void ws_server_request_shutdown(WebSocketServer* server) {
    if (server) {
        server->shutdown_requested = true; 
    }
}

// Process incoming connections and messages
void ws_server_process(WebSocketServer* server) {
    if (!server || server->shutdown_requested) {
        return;
    }

    fd_set read_fds;
    struct timeval tv;

    // Prepare file descriptor set
    FD_ZERO(&read_fds);
    FD_SET(server->server_socket, &read_fds);
    int max_fd = server->server_socket;

    // Add existing client sockets
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i] && server->clients[i]->socket > 0) {
            FD_SET(server->clients[i]->socket, &read_fds);
            max_fd = (server->clients[i]->socket > max_fd) ?
                      server->clients[i]->socket : max_fd;
        }
    }

    // Set timeout (100ms)
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms to allow more responsive shutdown

    // Wait for activity
    int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

    if (activity < 0 && errno != EINTR) {
        if (!server->shutdown_requested) {
            LOG_ERROR("Select error: %s", strerror(errno));
        }
        return;
    }

    // New connection
    if (activity > 0 && FD_ISSET(server->server_socket, &read_fds)) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_socket = accept(server->server_socket,
                                   (struct sockaddr*)&client_addr,
                                   &addr_len);

        if (client_socket < 0) {
            LOG_ERROR("Failed to accept client connection: %s", strerror(errno));
            return;
        }

        // Check if we have room for more clients
        if (server->client_count >= MAX_CLIENTS) {
            LOG_WARN("Maximum client limit reached. Rejecting connection.");
            close(client_socket);
            return;
        }

        // Create WebSocket client
        WebSocketClient* client = create_websocket_client(client_socket);
        if (!client) {
            close(client_socket);
            return;
        }

        // Store client and invoke connection callback
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!server->clients[i]) {
                server->clients[i] = client;
                server->client_count++;
                break;
            }
        }

        // Call connection callback
        if (server->config.on_client_connect) {
            server->config.on_client_connect(client);
        }
    }

    // Check for client messages
    for (int i = 0; i < MAX_CLIENTS; i++) {
        WebSocketClient* client = server->clients[i];
        if (!client || client->socket <= 0) continue;

        if (activity > 0 && FD_ISSET(client->socket, &read_fds)) {
            uint8_t buffer[BUFFER_SIZE];
            ssize_t bytes_read = recv(client->socket, buffer, sizeof(buffer) - 1, 0);

            if (bytes_read <= 0) {
                // Connection closed or error
                if (server->config.on_client_disconnect) {
                    server->config.on_client_disconnect(client);
                }

                // Remove client
                destroy_websocket_client(client);
                server->clients[i] = NULL;
                server->client_count--;
                continue;
            }

            buffer[bytes_read] = '\0';  // Null-terminate for string operations

            // Check for WebSocket handshake
            if (!client->handshake_complete &&
                portable_memmem(buffer, bytes_read, "GET", 3) &&
                portable_memmem(buffer, bytes_read, "Upgrade: websocket", 18)) {
                if (!perform_websocket_handshake(client, buffer, bytes_read)) {
                    LOG_ERROR("WebSocket handshake failed");
                    destroy_websocket_client(client);
                    server->clients[i] = NULL;
                    server->client_count--;
                }
                continue;
            }

            // Decode WebSocket frame if handshake is complete
            if (client->handshake_complete) {
                uint8_t* payload = NULL;
                size_t payload_len = 0;
                WebSocketFrameType frame_type;

                if (ws_frame_decode(buffer, bytes_read, 
                                    &payload, &payload_len, 
                                    &frame_type)) {
                    // Process the decoded payload
                    if (server->config.on_client_message) {
                        server->config.on_client_message(client, payload, payload_len);
                    }
                    
                    // Free the decoded payload
                    free(payload);
                } else {
                    LOG_ERROR("Failed to decode WebSocket frame");
                }
            }
        }
    }
}

// Broadcast message to all connected clients
void ws_server_broadcast(WebSocketServer* server, const uint8_t* data, size_t len) {
    if (!server || !data) return;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        WebSocketClient* client = server->clients[i];
        if (client && client->socket > 0 && client->handshake_complete) {
            ws_client_send(client, data, len);
        }
    }
}

// Destroy the WebSocket server
void ws_server_destroy(WebSocketServer* server) {
    if (!server) return;

    // Set shutdown flag
    server->shutdown_requested = true;

    // Close and free all clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i]) {
            destroy_websocket_client(server->clients[i]);
            server->clients[i] = NULL;
        }
    }

    // Close server socket
    if (server->server_socket > 0) {
        shutdown(server->server_socket, SHUT_RDWR);
        close(server->server_socket);
    }

    free(server);
}

// Send data to a specific client
void ws_client_send(WebSocketClient* client, const uint8_t* data, size_t len) {
    if (!client || !data || client->socket <= 0 || !client->handshake_complete) return;

    uint8_t* encoded_frame = NULL;
    size_t encoded_len = 0;

    // Encode the frame before sending
    if (ws_frame_encode(data, len, WS_FRAME_BINARY, &encoded_frame, &encoded_len)) {
        ssize_t sent = send(client->socket, encoded_frame, encoded_len, 0);
        
        // Free the encoded frame
        free(encoded_frame);

        if (sent < 0) {
            LOG_ERROR("Failed to send data to client: %s", strerror(errno));
        } else if ((size_t)sent != encoded_len) {
            LOG_WARN("Incomplete send: %zd of %zu bytes", sent, encoded_len);
        }
    } else {
        LOG_ERROR("Failed to encode WebSocket frame");
    }
}

// Close a specific client connection
void ws_client_close(WebSocketClient* client) {
    if (!client) return;

    if (client->socket > 0) {
        // Attempt to send a close frame
        uint8_t* close_frame = NULL;
        size_t close_frame_len = 0;

        if (ws_frame_encode(NULL, 0, WS_FRAME_CLOSE, &close_frame, &close_frame_len)) {
            send(client->socket, close_frame, close_frame_len, 0);
            free(close_frame);
        }

        // Shutdown and close socket
        shutdown(client->socket, SHUT_RDWR);
        close(client->socket);
        client->socket = -1;
    }
}

// Get user data associated with a client
void* ws_client_get_user_data(const WebSocketClient* client) {
    return client ? client->user_data : NULL;
}

// Set user data for a client
void ws_client_set_user_data(WebSocketClient* client, void* data) {
    if (client) {
        client->user_data = data;
    }
}
