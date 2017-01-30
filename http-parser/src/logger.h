//
// Created by s.fionov on 25.11.16.
//

#ifndef HTTP_PARSER_LOGGER_H
#define HTTP_PARSER_LOGGER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Log levels
 */
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3,
    LOG_LEVEL_TRACE = 4
} logger_log_level_t;

struct logger;
typedef struct logger logger;

/**
 * Logger callback type
 */
typedef void (*logger_callback_t)(logger *ctx, logger_log_level_t log_level, const char *thread_info, const char *message);

/**
 * Logger context definition
 */
struct logger {
    logger_callback_t callback_func;
    FILE *log_file;
    logger_log_level_t log_level;
    void *attachment;
};

/**
 * Initialize logger (open log file and set logger callback)
 * @param filename Filename
 * @param log_level Logger log level
 * @param callback Logger callback
 * @return Logger context
 */
extern logger *logger_open(const char *filename, logger_log_level_t log_level, logger_callback_t callback, void *attachment);

/**
 * Log message
 * @param ctx Logger context
 * @param log_level Log level
 * @param fmt Message format string
 * @param varargs... Message format string arguments
 */
extern void logger_log(logger *ctx, logger_log_level_t log_level, const char *fmt, ...);

/**
 * Logger set log level
 * @param ctx Logger context
 * @param log_level Logger log level
 */
extern void logger_set_log_level(logger *ctx, logger_log_level_t log_level);

/**
 * Is logger open
 * @param ctx Logger context
 * @return True if opened
 */
extern int logger_is_open(logger *ctx);

/**
 * Close logger
 * @param ctx Logger context
 * @return 0 if success
 */
int logger_close(logger *ctx);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* HTTP_PARSER_LOGGER_H */
