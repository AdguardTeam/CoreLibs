//
// Created by sw on 25.11.16.
//

#include <stddef.h>
#include "../src/logger.h"

int main() {
    logger_init(NULL, NULL);
    logger_log(0, "ddd");
}
