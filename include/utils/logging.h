#ifndef QUANT_TRADING_LOGGING_H
#define QUANT_TRADING_LOGGING_H

#include <stdio.h>
#include <time.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

#define LOG_DEBUG(fmt, ...) log_message(LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_message(LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) log_message(LOG_WARNING, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_message(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

void log_message(LogLevel level, const char* file, int line, const char* fmt, ...);
void set_log_level(LogLevel level);

#endif // QUANT_TRADING_LOGGING_H
