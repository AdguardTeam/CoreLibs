//
// Created by sw on 25.11.16.
//

#include <stddef.h>
#include <time.h>
#include "../src/logger.h"

int main() {
    logger_open(NULL, LOG_LEVEL_INFO, NULL);
    logger_log(LOG_LEVEL_INFO, "Logger test %d", time(0));
    logger_log(LOG_LEVEL_TRACE, "Logger test trace 1");
    logger_set_log_level(LOG_LEVEL_TRACE);
    logger_log(LOG_LEVEL_TRACE, "Logger test trace 2");
}
