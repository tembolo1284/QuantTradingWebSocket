#include "unity.h"
#include "net/websocket.h"
#include "net/frame.h"
#include <string.h>
#include <stdlib.h>

static WebSocket* ws;
static bool message_received;
static uint8_t received_data[1024];
static size_t received_len;

static void test_on_message(const uint8_t* data, size_t len, void* user_data) {
    (void)user_data;  // Mark as intentionally unused
    message_received = true;
    received_len = len < sizeof(received_data) ? len : sizeof(received_data);
    memcpy(received_data, data, received_len);
}

static void test_on_error(ErrorCode error, void* user_data) {
    (void)user_data;  // Mark as intentionally unused
    printf("WebSocket error: %d\n", error);
}

void setUp(void) {
    WebSocketCallbacks callbacks = {
        .on_message = test_on_message,
        .on_error = test_on_error,
        .user_data = NULL
    };
    
    ws = ws_create("127.0.0.1", 8080, &callbacks);
    message_received = false;
    memset(received_data, 0, sizeof(received_data));
    received_len = 0;
}

void tearDown(void) {
    ws_close(ws);
}

void test_websocket_creation(void) {
    TEST_ASSERT_NOT_NULL(ws);
}

void test_websocket_send_receive(void) {
    const uint8_t test_data[] = "Hello, WebSocket!";
    TEST_ASSERT_TRUE(ws_send(ws, test_data, sizeof(test_data)));
}

void test_frame_creation(void) {
    const uint8_t payload[] = "Test payload";
    WebSocketFrame frame = {0};
    
    TEST_ASSERT_EQUAL_INT(0, frame_create(&frame, payload, sizeof(payload), FRAME_TEXT));
    TEST_ASSERT_EQUAL_INT(FRAME_TEXT, frame.type);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), frame.payload_length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, frame.payload, sizeof(payload));
    
    free(frame.payload);
}

void test_frame_types(void) {
    const FrameType types[] = {
        FRAME_TEXT,
        FRAME_BINARY,
        FRAME_PING,
        FRAME_PONG
    };
    const uint8_t payload[] = "Test";
    
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        WebSocketFrame frame = {0};
        TEST_ASSERT_EQUAL_INT(0, frame_create(&frame, payload, sizeof(payload), types[i]));
        TEST_ASSERT_EQUAL_INT(types[i], frame.type);
        TEST_ASSERT_EQUAL_size_t(sizeof(payload), frame.payload_length);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, frame.payload, sizeof(payload));
        free(frame.payload);
    }
}

void test_frame_parsing(void) {
    const uint8_t raw_frame[] = {
        0x82, 0x04,           // Binary frame, 4 bytes payload
        0x74, 0x65, 0x73, 0x74 // "test" payload
    };
    
    WebSocketFrame frame = {0};
    int result = frame_parse(raw_frame, sizeof(raw_frame), &frame);
    
    TEST_ASSERT_GREATER_THAN(0, result);
    TEST_ASSERT_EQUAL_INT(FRAME_BINARY, frame.type);
    TEST_ASSERT_EQUAL_size_t(4, frame.payload_length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)"test", frame.payload, 4);
    
    free(frame.payload);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_websocket_creation);
    RUN_TEST(test_websocket_send_receive);
    RUN_TEST(test_frame_creation);
    RUN_TEST(test_frame_types);
    RUN_TEST(test_frame_parsing);
    return UNITY_END();
}
