#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "net/websocket.h"
#include "net/frame.h"

static WebSocket* ws;
static bool message_received;
static uint8_t received_data[1024];
static size_t received_len;

static void test_on_message(const uint8_t* data, size_t len, void* user_data) {
    message_received = true;
    received_len = len < sizeof(received_data) ? len : sizeof(received_data);
    memcpy(received_data, data, received_len);
}

static void test_on_error(ErrorCode error, void* user_data) {
    fprintf(stderr, "WebSocket error: %d\n", error);
}

void websocket_setup(void) {
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

void websocket_teardown(void) {
    ws_close(ws);
}

Test(websocket, creation, .init = websocket_setup, .fini = websocket_teardown) {
    cr_assert(ws != NULL, "WebSocket should be created successfully");
}

Test(websocket, send_receive, .init = websocket_setup, .fini = websocket_teardown) {
    const uint8_t test_data[] = "Hello, WebSocket!";
    cr_assert(ws_send(ws, test_data, sizeof(test_data)), 
              "Should send data successfully");
    
    // In a real test, we'd need a mock server to echo the data back
    // For now, we just verify the send operation succeeded
}

Test(websocket, frame_parsing) {
    // Test frame creation
    const uint8_t payload[] = "Test payload";
    WebSocketFrame frame = {0};
    
    cr_assert_eq(frame_create(&frame, payload, sizeof(payload), FRAME_TEXT), 0,
                 "Should create frame successfully");
    
    cr_assert_eq(frame.type, FRAME_TEXT, "Frame type should be TEXT");
    cr_assert_eq(frame.payload_length, sizeof(payload),
                 "Payload length should match input");
    cr_assert_arr_eq(frame.payload, payload, sizeof(payload),
                    "Payload should match input");
    
    free(frame.payload);
}

// Parameterized test for different frame types
ParameterizedTestParameters(websocket, frame_types) {
    static struct {
        FrameType type;
        const char* desc;
    } params[] = {
        { FRAME_TEXT, "text frame" },
        { FRAME_BINARY, "binary frame" },
        { FRAME_PING, "ping frame" },
        { FRAME_PONG, "pong frame" }
    };
    
    return cr_make_param_array(
        struct { FrameType type; const char* desc; },
        params, sizeof(params) / sizeof(params[0])
    );
}

ParameterizedTest(struct { FrameType type; const char* desc; }* param,
                 websocket, frame_types) {
    WebSocketFrame frame = {0};
    const uint8_t payload[] = "Test";
    
    cr_assert_eq(frame_create(&frame, payload, sizeof(payload), param->type), 0,
                 "Should create %s successfully", param->desc);
    cr_assert_eq(frame.type, param->type,
                 "Frame type should match for %s", param->desc);
                 
    free(frame.payload);
}
