#ifndef QUANT_TRADING_COMMON_H
#define QUANT_TRADING_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Common error codes for all components
typedef enum ErrorCode {
    ERROR_NONE = 0,
    ERROR_INVALID_PARAM,
    ERROR_MEMORY,
    ERROR_NETWORK,
    ERROR_TIMEOUT,
    // WebSocket specific errors
    ERROR_WS_CONNECTION_FAILED,
    ERROR_WS_HANDSHAKE_FAILED,
    ERROR_WS_INVALID_FRAME,
    ERROR_WS_SEND_FAILED,
    // Trading specific errors
    ERROR_TRADING_INVALID_ORDER,
    ERROR_TRADING_BOOK_FULL,
    ERROR_TRADING_ORDER_NOT_FOUND
} ErrorCode;

// Timestamp in nanoseconds
typedef uint64_t timestamp_t;

// Get current timestamp in nanoseconds
static inline timestamp_t get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#endif // QUANT_TRADING_COMMON_H
