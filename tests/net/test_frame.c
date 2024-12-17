#include "unity.h"
#include "net/frame.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {
    // Nothing to initialize
}

void tearDown(void) {
    // Nothing to clean up
}

void test_frame_create(void) {
    const uint8_t data[] = "Hello, WebSocket!";
    WebSocketFrame* frame = frame_create(data, sizeof(data), FRAME_TEXT);
    
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_TRUE(frame->header.fin);
    TEST_ASSERT_EQUAL_INT(FRAME_TEXT, frame->header.opcode);
    TEST_ASSERT_EQUAL_UINT64(sizeof(data), frame->header.payload_len);
    TEST_ASSERT_EQUAL_MEMORY(data, frame->payload, sizeof(data));
    
    frame_destroy(frame);
}

void test_frame_encode_decode(void) {
    const uint8_t data[] = "Test Message";
    WebSocketFrame* original = frame_create(data, sizeof(data), FRAME_TEXT);
    
    size_t encoded_len;
    uint8_t* encoded = frame_encode(original, &encoded_len);
    TEST_ASSERT_NOT_NULL(encoded);
    
    WebSocketFrame* decoded = NULL;
    FrameParseResult result = frame_parse(encoded, encoded_len, &decoded);
    
    TEST_ASSERT_TRUE(result.complete);
    TEST_ASSERT_EQUAL_UINT64(encoded_len, result.bytes_consumed);
    TEST_ASSERT_NULL(result.error);
    TEST_ASSERT_NOT_NULL(decoded);
    
    TEST_ASSERT_EQUAL_INT(original->header.opcode, decoded->header.opcode);
    TEST_ASSERT_EQUAL_UINT64(original->header.payload_len, decoded->header.payload_len);
    TEST_ASSERT_EQUAL_MEMORY(original->payload, decoded->payload, original->header.payload_len);
    
    frame_destroy(original);
    frame_destroy(decoded);
    free(encoded);
}

void test_frame_control_messages(void) {
    // Test PING frame
    WebSocketFrame* ping = frame_create((uint8_t*)"PING", 4, FRAME_PING);
    TEST_ASSERT_NOT_NULL(ping);
    TEST_ASSERT_TRUE(frame_validate(ping));
    frame_destroy(ping);
    
    // Test PONG frame
    WebSocketFrame* pong = frame_create((uint8_t*)"PONG", 4, FRAME_PONG);
    TEST_ASSERT_NOT_NULL(pong);
    TEST_ASSERT_TRUE(frame_validate(pong));
    frame_destroy(pong);
    
    // Test CLOSE frame
    WebSocketFrame* close = frame_create(NULL, 0, FRAME_CLOSE);
    TEST_ASSERT_NOT_NULL(close);
    TEST_ASSERT_TRUE(frame_validate(close));
    frame_destroy(close);
}

void test_frame_validation(void) {
    // Test invalid control frame (too large)
    uint8_t large_data[126];
    memset(large_data, 'A', sizeof(large_data));
    WebSocketFrame* invalid_ping = frame_create(large_data, sizeof(large_data), FRAME_PING);
    TEST_ASSERT_FALSE(frame_validate(invalid_ping));
    frame_destroy(invalid_ping);
    
    // Test invalid opcode
    WebSocketFrame invalid_frame = {
        .header = {
            .fin = true,
            .opcode = 15,  // Invalid opcode
            .payload_len = 0
        },
        .payload = NULL
    };
    TEST_ASSERT_FALSE(frame_validate(&invalid_frame));
}

void test_frame_fragmentation(void) {
    const char* message = "This is a fragmented message";
    size_t msg_len = strlen(message);
    size_t part1_len = 10;
    
    // Create first fragment
    WebSocketFrame* frag1 = frame_create((uint8_t*)message, part1_len, FRAME_TEXT);
    frag1->header.fin = false;  // Mark as fragment
    TEST_ASSERT_TRUE(frame_validate(frag1));
    
    // Create continuation fragment
    WebSocketFrame* frag2 = frame_create((uint8_t*)(message + part1_len), 
                                       msg_len - part1_len, FRAME_CONTINUATION);
    TEST_ASSERT_TRUE(frame_validate(frag2));
    
    frame_destroy(frag1);
    frame_destroy(frag2);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_frame_create);
    RUN_TEST(test_frame_encode_decode);
    RUN_TEST(test_frame_control_messages);
    RUN_TEST(test_frame_validation);
    RUN_TEST(test_frame_fragmentation);
    
    return UNITY_END();
}
