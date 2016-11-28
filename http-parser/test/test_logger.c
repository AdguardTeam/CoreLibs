//
// Created by sw on 25.11.16.
//

#include <stddef.h>
#include <time.h>
#include "../src/logger.h"

int main() {
    logger_init(NULL, NULL);
    logger_log(LOG_LEVEL_INFO, "Logger test %d", time(0));
}
