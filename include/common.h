// include/common.h
#ifndef QUANT_TRADING_COMMON_H
#define QUANT_TRADING_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Error codes
typedef enum {
    ERROR_NONE = 0,
    ERROR_INVALID_PARAM = -1,
    ERROR_MEMORY = -2,
    ERROR_NETWORK = -3,
    ERROR_TIMEOUT = -4
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
