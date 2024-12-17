#include "unity.h"
#include "net/socket.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static SocketOptions test_options;

void setUp(void) {
    socket_init_options(&test_options);
}

void tearDown(void) {
    // Nothing to clean up
}

void test_socket_options_init(void) {
    SocketOptions options;
    socket_init_options(&options);
    
    TEST_ASSERT_TRUE(options.non_blocking);
    TEST_ASSERT_TRUE(options.tcp_nodelay);
    TEST_ASSERT_TRUE(options.keep_alive);
    TEST_ASSERT_EQUAL_UINT32(5000, options.connect_timeout_ms);
}

void test_socket_create_and_connect_invalid_host(void) {
    SocketResult result = socket_create_and_connect("invalid.host.local", 8080, &test_options);
    
    TEST_ASSERT_LESS_THAN(0, result.fd);
    TEST_ASSERT_NOT_NULL(result.error_message);
    free((void*)result.error_message);
}

void test_socket_create_and_connect_invalid_port(void) {
    SocketResult result = socket_create_and_connect("localhost", 0, &test_options);
    
    TEST_ASSERT_LESS_THAN(0, result.fd);
    TEST_ASSERT_NOT_NULL(result.error_message);
    free((void*)result.error_message);
}

void test_socket_configure(void) {
    SocketResult result = socket_create_and_connect("localhost", 8080, NULL);
    if (result.fd >= 0) {  // Only test if we can connect
        TEST_ASSERT_TRUE(socket_configure(result.fd, &test_options));
        close(result.fd);
    }
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_socket_options_init);
    RUN_TEST(test_socket_create_and_connect_invalid_host);
    RUN_TEST(test_socket_create_and_connect_invalid_port);
    RUN_TEST(test_socket_configure);
    
    return UNITY_END();
}
