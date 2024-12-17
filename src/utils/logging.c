#include "utils/logging.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static LogLevel current_log_level = LOG_DEBUG;
static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR"
};

static const char* get_filename(const char* path) {
    const char* filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}

void log_message(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < current_log_level) return;

    time_t now;
    time(&now);
    struct tm* local_time = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local_time);

    // Print timestamp, level, and location
    fprintf(stderr, "[%s] [%s] [%s:%d] ", 
            timestamp, level_strings[level], get_filename(file), line);

    // Print the actual message
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void set_log_level(LogLevel level) {
    current_log_level = level;
}
