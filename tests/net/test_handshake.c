#include "unity.h"
#include "net/handshake.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

static HandshakeConfig test_config;

void setUp(void) {
    handshake_init_config(&test_config);
    test_config.host = "localhost";
    test_config.port = 8080;
}

void tearDown(void) {
    // Nothing to clean up
}

void test_handshake_config_init(void) {
    HandshakeConfig config;
    handshake_init_config(&config);
    
    TEST_ASSERT_EQUAL_STRING("/", config.path);
    TEST_ASSERT_NULL(config.protocol);
    TEST_ASSERT_NULL(config.origin);
}

void test_handshake_key_generation(void) {
    char* key = handshake_generate_key();
    TEST_ASSERT_NOT_NULL(key);
    
    // Check key length (should be 24 characters for base64 encoded 16 bytes)
    TEST_ASSERT_EQUAL_INT(24, strlen(key));
    
    // Check that key contains only valid base64 characters
    const char* valid_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    for (size_t i = 0; i < strlen(key); i++) {
        TEST_ASSERT_NOT_NULL(strchr(valid_chars, key[i]));
    }
    
    free(key);
}

void test_handshake_response_validation(void) {
    char* key = handshake_generate_key();
    TEST_ASSERT_NOT_NULL(key);
    
    // Create a mock response
    char response[1024];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        "dGhlIHNhbXBsZSBub25jZQ==");  // Example accept key
    
    // Test validation
    TEST_ASSERT_FALSE(handshake_validate_response(response, key));
    
    free(key);
}

void test_handshake_cleanup(void) {
    HandshakeResult result = {
        .success = false,
        .error_message = strdup("Test error"),
        .accept_key = strdup("Test key"),
        .protocol = strdup("Test protocol"),
        .extensions = strdup("Test extensions")
    };
    
    handshake_cleanup_result(&result);
    
    // All pointers should be NULL after cleanup
    TEST_ASSERT_NULL(result.error_message);
    TEST_ASSERT_NULL(result.accept_key);
    TEST_ASSERT_NULL(result.protocol);
    TEST_ASSERT_NULL(result.extensions);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_handshake_config_init);
    RUN_TEST(test_handshake_key_generation);
    RUN_TEST(test_handshake_response_validation);
    RUN_TEST(test_handshake_cleanup);
    
    return UNITY_END();
}
