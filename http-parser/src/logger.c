//
// Created by s.fionov on 25.11.16.
//

// for pthread_getname_np()
#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <syscall.h>
#include <stdarg.h>
#include "logger.h"

#define TIME_FORMAT "%d.%m.%Y %H:%M:%S%z"

static logger_callback_t logger_callback_func = NULL;

static FILE * logger_file = NULL;
static logger_log_level_t logger_current_log_level = LOG_LEVEL_INFO;

static const char *log_level_name(int level);
static void logger_log_to_file(logger_log_level_t log_level, const char *message);

void logger_open(const char *filename, logger_log_level_t log_level, logger_callback_t callback) {
    if (callback) {
        logger_callback_func = callback;
    } else {
        if (filename) {
            logger_file = fopen(filename, "w+");
            if (logger_file == NULL) {
                fprintf(stderr, "Error opening logfile \"%s\":%s\n", filename, strerror(errno));
            }
        }
        if (logger_file == NULL) {
            fprintf(stderr, "Using stderr for log output.\n");
            logger_file = stderr;//fdopen(dup(fileno(stderr)), "w+");
        }
        logger_callback_func = logger_log_to_file;
    }
    logger_current_log_level = log_level;
}

void logger_close() {
    if (logger_file) {
        if (fileno(logger_file) < 3 && fclose(logger_file) != 0) {
            fprintf(stderr, "Error closing logfile");
        }
        logger_file = NULL;
        logger_callback_func = NULL;
    }
}

static void logger_log_to_file(logger_log_level_t log_level, const char *message) {
    if (logger_file != NULL) {
        time_t now = time(0);
        char time_str[26];
        char thread_name[50];
        strftime(time_str, 26, TIME_FORMAT, localtime(&now));
        if (pthread_getname_np(pthread_self(), thread_name, 50) == ERANGE) {
            thread_name[0] = '\0';
        }
        fprintf(logger_file, "%s %s[%ld] %s %s\n", time_str, thread_name, syscall(SYS_gettid),
                log_level_name(log_level), message);
    }
}

static const char *log_level_name(int level) {
    switch (level) {
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_WARN:
            return "WARN";
        default:
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_TRACE:
            return "TRACE";
    }
}

/**
 * 
 * @param log_level
 * @param message
 */
void logger_log(logger_log_level_t log_level, const char *message, ...) {
    va_list args;
    va_start(args, message);
    if (logger_callback_func && log_level <= logger_current_log_level) {
        char fmt_message[256];
        vsnprintf(fmt_message, 256, message, args);
        logger_callback_func(log_level, fmt_message);
    }
    va_end(args);
}

void logger_set_log_level(logger_log_level_t log_level) {
    logger_current_log_level = log_level;
}

int logger_is_open() {
    return logger_file != NULL;
}
