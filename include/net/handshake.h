#ifndef QUANT_TRADING_HANDSHAKE_H
#define QUANT_TRADING_HANDSHAKE_H

#include <stdint.h>
#include <stdbool.h>

// Handshake result structure
typedef struct {
    bool success;
    char* error_message;
    char* accept_key;      // Server's Sec-WebSocket-Accept key
    char* protocol;        // Negotiated protocol (if any)
    char* extensions;      // Negotiated extensions (if any)
} HandshakeResult;

// Handshake configuration
typedef struct {
    const char* host;
    uint16_t port;
    const char* path;
    const char* protocol;
    const char* origin;
} HandshakeConfig;

// Initialize handshake configuration with defaults
void handshake_init_config(HandshakeConfig* config);

// Perform WebSocket handshake
HandshakeResult handshake_perform(int sock_fd, const HandshakeConfig* config);

// Generate WebSocket key
char* handshake_generate_key(void);

// Validate server's handshake response
bool handshake_validate_response(const char* response, const char* sent_key);

// Clean up handshake result
void handshake_cleanup_result(HandshakeResult* result);

#endif // QUANT_TRADING_HANDSHAKE_H
