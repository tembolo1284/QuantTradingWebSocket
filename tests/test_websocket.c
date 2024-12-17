#include "unity.h"
#include "net/websocket.h"
#include "net/frame.h"
#include <stdlib.h>
#include <string.h>

static WebSocket* ws;
static bool message_received;
static uint8_t received_data[1024];
static size_t received_len;

static void test_on_message(const uint8_t* data, size_t len, void* user_data) {
    (void)user_data;  // Unused parameter
    message_received = true;
    received_len = len < sizeof(received_data) ? len : sizeof(received_data);
    memcpy(received_data, data, received_len);
}

static void test_on_error(ErrorCode error, void* user_data) {
    (void)user_data;  // Unused parameter
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
    WebSocketFrame* frame = frame_create(payload, sizeof(payload), FRAME_TEXT);
    
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_INT(FRAME_TEXT, frame->header.opcode);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), frame->header.payload_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, frame->payload, sizeof(payload));
    
    frame_destroy(frame);
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
        WebSocketFrame* frame = frame_create(payload, sizeof(payload), types[i]);
        TEST_ASSERT_NOT_NULL(frame);
        TEST_ASSERT_EQUAL_INT(types[i], frame->header.opcode);
        TEST_ASSERT_EQUAL_size_t(sizeof(payload), frame->header.payload_len);
        TEST_ASSERT_EQUAL_MEMORY(payload, frame->payload, sizeof(payload));
        frame_destroy(frame);
    }
}

void test_frame_parsing(void) {
    const uint8_t raw_frame[] = {
        0x82, 0x04,           // Binary frame, 4 bytes payload
        0x74, 0x65, 0x73, 0x74 // "test" payload
    };
    
    WebSocketFrame* frame = NULL;
    FrameParseResult result = frame_parse(raw_frame, sizeof(raw_frame), &frame);
    
    TEST_ASSERT_TRUE(result.complete);
    TEST_ASSERT_NULL(result.error);
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_INT(FRAME_BINARY, frame->header.opcode);
    TEST_ASSERT_EQUAL_size_t(4, frame->header.payload_len);
    TEST_ASSERT_EQUAL_MEMORY("test", frame->payload, 4);
    
    frame_destroy(frame);
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
