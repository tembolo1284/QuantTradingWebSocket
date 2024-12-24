#include "common/types.h"

const char* error_code_to_string(ErrorCode error) {
    switch (error) {
        case ERROR_NONE: return "No error";
        case ERROR_INVALID_PARAM: return "Invalid parameter";
        case ERROR_MEMORY: return "Memory allocation failed";
        case ERROR_NETWORK: return "Network error";
        case ERROR_TIMEOUT: return "Operation timed out";
        case ERROR_WS_CONNECTION_FAILED: return "WebSocket connection failed";
        case ERROR_WS_HANDSHAKE_FAILED: return "WebSocket handshake failed";
        case ERROR_WS_INVALID_FRAME: return "Invalid WebSocket frame";
        case ERROR_WS_SEND_FAILED: return "WebSocket send failed";
        case ERROR_TRADING_INVALID_ORDER: return "Invalid trading order";
        case ERROR_TRADING_BOOK_FULL: return "Trading book is full";
        case ERROR_TRADING_ORDER_NOT_FOUND: return "Trading order not found";
        default: return "Unknown error";
    }
}
