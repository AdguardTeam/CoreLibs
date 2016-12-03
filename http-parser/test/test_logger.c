//
// Created by s.fionov on 25.11.16.
//

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <memory.h>
#include <unistd.h>

#include "logger.h"

int main() {
    // Generate output file name
    char file_name[L_tmpnam];
    tmpnam(file_name);

    // Open logger
    logger *log = logger_open(file_name, LOG_LEVEL_INFO, NULL, NULL);
    assert(log != NULL);

    // Log some messages
    logger_log(log, LOG_LEVEL_INFO, "Logger test info %d", time(0));
    logger_log(log, LOG_LEVEL_TRACE, "Logger test trace 1");
    logger_set_log_level(log, LOG_LEVEL_TRACE);
    logger_log(log, LOG_LEVEL_TRACE, "Logger test trace 2");
    logger_close(log);

    FILE *file = fopen(file_name, "r");
    char line[128];
    int linenum = 0;
    while (fgets(line, 128, file) != NULL) {
        ++linenum;
        // Line 1 contains "test info"
        assert(linenum != 1 || strstr(line, "test info"));
        // Line 2 contains "test trace 2"
        assert(linenum != 2 || strstr(line, "test trace 2"));
    }
    // Output file has 2 lines
    assert(linenum == 2);
    fclose(file);
    unlink(file_name);
}
