#include "net/handshake.h"
#include "utils/logging.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define MAX_RESPONSE_SIZE 4096
#define TIMEOUT_SEC 5

void handshake_init_config(HandshakeConfig* config) {
    LOG_DEBUG("Initializing handshake configuration");
    config->path = "/";
    config->protocol = NULL;
    config->origin = NULL;
    LOG_DEBUG("Handshake config initialized with path=%s", config->path);
}

static char* base64_encode(const unsigned char* input, size_t length) {
    LOG_DEBUG("Base64 encoding %zu bytes", length);
    
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    if (!b64) {
        LOG_ERROR("Failed to create base64 BIO");
        return NULL;
    }
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    bmem = BIO_new(BIO_s_mem());
    if (!bmem) {
        LOG_ERROR("Failed to create memory BIO");
        BIO_free(b64);
        return NULL;
    }
    
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char* encoded = malloc(bptr->length + 1);
    if (!encoded) {
        LOG_ERROR("Failed to allocate memory for encoded data");
        BIO_free_all(b64);
        return NULL;
    }
    
    memcpy(encoded, bptr->data, bptr->length);
    encoded[bptr->length] = 0;
    
    BIO_free_all(b64);
    
    LOG_DEBUG("Base64 encoding successful, result length: %zu", strlen(encoded));
    return encoded;
}

char* handshake_generate_key(void) {
    LOG_DEBUG("Generating WebSocket key");
    
    unsigned char random[16];
    for (int i = 0; i < 16; i++) {
        random[i] = rand() & 0xFF;
    }
    
    char* key = base64_encode(random, 16);
    if (key) {
        LOG_DEBUG("Generated WebSocket key: %s", key);
    } else {
        LOG_ERROR("Failed to generate WebSocket key");
    }
    
    return key;
}

static char* generate_accept_key(const char* key) {
    LOG_DEBUG("Generating accept key for websocket key: %s", key);
    
    // Concatenate key with GUID
    char concat[256];
    snprintf(concat, sizeof(concat), "%s%s", key, GUID);
    
    // Calculate SHA1
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)concat, strlen(concat), hash);
    
    // Base64 encode
    char* accept = base64_encode(hash, SHA_DIGEST_LENGTH);
    if (accept) {
        LOG_DEBUG("Generated accept key: %s", accept);
    } else {
        LOG_ERROR("Failed to generate accept key");
    }
    
    return accept;
}

static bool wait_for_response(int sock_fd, char* response, size_t max_size) {
    LOG_DEBUG("Waiting for handshake response");
    
    size_t total = 0;
    time_t start = time(NULL);
    
    while (total < max_size) {
        if (time(NULL) - start > TIMEOUT_SEC) {
            LOG_ERROR("Handshake response timeout after %d seconds", TIMEOUT_SEC);
            return false;
        }
        
        ssize_t received = read(sock_fd, response + total, max_size - total);
        
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Non-blocking socket, wait a bit
                LOG_DEBUG("Socket would block, waiting...");
                usleep(100000);  // 100ms
                continue;
            }
            LOG_ERROR("Failed to read handshake response: %s", strerror(errno));
            return false;
        }
        
        if (received == 0) {
            LOG_ERROR("Connection closed during handshake");
            return false;
        }
        
        total += received;
        response[total] = '\0';
        
        LOG_DEBUG("Received %zd bytes, total %zu bytes", received, total);
        
        // Check if we have a complete response
        if (strstr(response, "\r\n\r\n")) {
            LOG_DEBUG("Found end of handshake response");
            return true;
        }
    }
    
    LOG_ERROR("Response too large (max %zu bytes)", max_size);
    return false;
}

HandshakeResult handshake_perform(int sock_fd, const HandshakeConfig* config) {
    LOG_INFO("Starting WebSocket handshake with %s:%u", config->host, config->port);
    
    HandshakeResult result = {0};
    char* key = handshake_generate_key();
    if (!key) {
        result.error_message = strdup("Failed to generate WebSocket key");
        return result;
    }
    
    // Format handshake request
    char request[1024];
    int len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n",
        config->path, config->host, config->port, key);
    
    // Add optional fields
    if (config->protocol) {
        len += snprintf(request + len, sizeof(request) - len,
            "Sec-WebSocket-Protocol: %s\r\n", config->protocol);
    }
    if (config->origin) {
        len += snprintf(request + len, sizeof(request) - len,
            "Origin: %s\r\n", config->origin);
    }
    
    // Add final CRLF
    strncat(request, "\r\n", sizeof(request) - len - 1);
    
    LOG_DEBUG("Sending handshake request:\n%s", request);
    
    // Send request
    ssize_t sent = write(sock_fd, request, strlen(request));
    if (sent != (ssize_t)strlen(request)) {
        LOG_ERROR("Failed to send handshake request: %s", strerror(errno));
        free(key);
        result.error_message = strdup(strerror(errno));
        return result;
    }
    
    // Wait for response
    char response[MAX_RESPONSE_SIZE] = {0};
    if (!wait_for_response(sock_fd, response, sizeof(response) - 1)) {
        free(key);
        result.error_message = strdup("Failed to receive handshake response");
        return result;
    }
    
    LOG_DEBUG("Received handshake response:\n%s", response);
    
    // Validate response
    if (!strstr(response, "HTTP/1.1 101")) {
        LOG_ERROR("Invalid response status (expected 101)");
        free(key);
        result.error_message = strdup("Invalid handshake response status");
        return result;
    }
    
    char* expected_accept = generate_accept_key(key);
    free(key);
    
    if (!expected_accept) {
        result.error_message = strdup("Failed to generate accept key");
        return result;
    }
    
    // Extract actual accept key from response
    char* accept_line = strstr(response, "Sec-WebSocket-Accept: ");
    if (!accept_line) {
        LOG_ERROR("Missing Sec-WebSocket-Accept header");
        free(expected_accept);
        result.error_message = strdup("Missing accept key in response");
        return result;
    }
    
    accept_line += strlen("Sec-WebSocket-Accept: ");
    char* end = strstr(accept_line, "\r\n");
    if (!end) {
        LOG_ERROR("Malformed accept key in response");
        free(expected_accept);
        result.error_message = strdup("Malformed accept key");
        return result;
    }
    
    size_t accept_len = end - accept_line;
    result.accept_key = malloc(accept_len + 1);
    if (!result.accept_key) {
        LOG_ERROR("Failed to allocate memory for accept key");
        free(expected_accept);
        result.error_message = strdup("Memory allocation failed");
        return result;
    }
    
    strncpy(result.accept_key, accept_line, accept_len);
    result.accept_key[accept_len] = '\0';
    
    // Compare keys
    if (strcmp(result.accept_key, expected_accept) != 0) {
        LOG_ERROR("Invalid accept key. Expected: %s, Got: %s", 
                 expected_accept, result.accept_key);
        free(expected_accept);
        result.error_message = strdup("Invalid accept key");
        return result;
    }
    
    free(expected_accept);
    
    // Extract protocol if present
    char* protocol_line = strstr(response, "Sec-WebSocket-Protocol: ");
    if (protocol_line) {
        protocol_line += strlen("Sec-WebSocket-Protocol: ");
        end = strstr(protocol_line, "\r\n");
        if (end) {
            size_t protocol_len = end - protocol_line;
            result.protocol = malloc(protocol_len + 1);
            if (result.protocol) {
                strncpy(result.protocol, protocol_line, protocol_len);
                result.protocol[protocol_len] = '\0';
                LOG_DEBUG("Negotiated protocol: %s", result.protocol);
            }
        }
    }
    
    LOG_INFO("WebSocket handshake completed successfully");
    result.success = true;
    return result;
}

bool handshake_validate_response(const char* response, const char* sent_key) {
    LOG_DEBUG("Validating handshake response against sent key: %s", sent_key);
    
    char* expected_accept = generate_accept_key(sent_key);
    if (!expected_accept) {
        LOG_ERROR("Failed to generate expected accept key");
        return false;
    }
    
    char* accept_line = strstr(response, "Sec-WebSocket-Accept: ");
    if (!accept_line) {
        LOG_ERROR("Missing Sec-WebSocket-Accept header");
        free(expected_accept);
        return false;
    }
    
    accept_line += strlen("Sec-WebSocket-Accept: ");
    char actual_accept[256] = {0};
    sscanf(accept_line, "%255[^\r\n]", actual_accept);
    
    bool valid = (strcmp(actual_accept, expected_accept) == 0);
    if (!valid) {
        LOG_ERROR("Accept key mismatch. Expected: %s, Got: %s",
                 expected_accept, actual_accept);
    } else {
        LOG_DEBUG("Handshake response validation successful");
    }
    
    free(expected_accept);
    return valid;
}

void handshake_cleanup_result(HandshakeResult* result) {
    if (!result) return;
    
    LOG_DEBUG("Cleaning up handshake result");
    free(result->error_message);
    free(result->accept_key);
    free(result->protocol);
    free(result->extensions);
    
    memset(result, 0, sizeof(HandshakeResult));
}
