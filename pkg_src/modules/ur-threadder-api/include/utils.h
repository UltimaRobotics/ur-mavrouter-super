/**
 * @file utils.h
 * @brief Utility functions for thread management API
 */

#ifndef UTILS_H
#define UTILS_H

/* Ensure proper header inclusion for POSIX features like nanosleep and strdup */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>   /* For kill() */
#include <string.h>   /* For strdup() */

/**
 * @brief Debug log levels
 */
typedef enum {
    LOG_LEVEL_DEBUG,  /**< Debug log level */
    LOG_LEVEL_INFO,   /**< Info log level */
    LOG_LEVEL_WARN,   /**< Warning log level */
    LOG_LEVEL_ERROR   /**< Error log level */
} log_level_t__;

/**
 * @brief Set log level
 * 
 * @param level Log level
 */
void set_log_level(log_level_t__ level);

/**
 * @brief Get current log level
 * 
 * @return log_level_t__ Current log level
 */
log_level_t__ get_log_level(void);

/**
 * @brief Log message
 * 
 * @param level Log level
 * @param file File name
 * @param line Line number
 * @param function Function name
 * @param format Format string
 * @param ... Format arguments
 */
void log_message(log_level_t__ level, const char *file, int line, const char *function, const char *format, ...);

/**
 * @brief Helper macros for variadic log message expansion
 */
#define DEBUG_LOG_IMPL(format, ...) log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, format, __VA_ARGS__)
#define INFO_LOG_IMPL(format, ...) log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, format, __VA_ARGS__)
#define WARN_LOG_IMPL(format, ...) log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, format, __VA_ARGS__)
#define ERROR_LOG_IMPL(format, ...) log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, format, __VA_ARGS__)

/**
 * @brief Debug log macro
 */
#define DEBUG_LOG(...) DEBUG_LOG_IMPL(__VA_ARGS__, "")

/**
 * @brief Info log macro
 */
#define INFO_LOG(...) INFO_LOG_IMPL(__VA_ARGS__, "")

/**
 * @brief Warning log macro
 */
#define WARN_LOG(...) WARN_LOG_IMPL(__VA_ARGS__, "")

/**
 * @brief Error log macro
 */
#define ERROR_LOG(...) ERROR_LOG_IMPL(__VA_ARGS__, "")

/**
 * @brief Get current time as string
 * 
 * @param buffer Buffer to store time string
 * @param size Buffer size
 * @return char* Buffer pointer
 */
char *get_time_string(char *buffer, size_t size);

#endif /* UTILS_H */
