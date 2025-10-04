/**
 * @file utils.c
 * @brief Implementation of utility functions
 */

#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

static log_level_t__ current_log_level = LOG_LEVEL_INFO;

void set_log_level(log_level_t__ level) {
    current_log_level = level;
}

log_level_t__ get_log_level(void) {
    return current_log_level;
}

char *get_time_string(char *buffer, size_t size) {
    time_t raw_time;
    struct tm *time_info;
    
    time(&raw_time);
    time_info = localtime(&raw_time);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", time_info);
    
    return buffer;
}

void log_message(log_level_t__ level, const char *file, int line, const char *function, const char *format, ...) {
    if (level < current_log_level) {
        return;
    }
    
    char time_buffer[64];
    get_time_string(time_buffer, sizeof(time_buffer));
    
    const char *level_str = "UNKNOWN";
    switch (level) {
        case LOG_LEVEL_DEBUG:
            level_str = "DEBUG";
            break;
        case LOG_LEVEL_INFO:
            level_str = "INFO";
            break;
        case LOG_LEVEL_WARN:
            level_str = "WARN";
            break;
        case LOG_LEVEL_ERROR:
            level_str = "ERROR";
            break;
    }
    
    fprintf(stderr, "[%s] [%s] [%s:%d:%s] ", time_buffer, level_str, file, line, function);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}
