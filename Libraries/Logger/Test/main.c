#include "logger.h"

#include <stdio.h>

int main(void)
{
    if (logger_init("../../Logs/logger_test.log", 1) != 0)
    {
        printf("Failed to initialize logger\n");
        return 1;
    }

    LOG_INFO("Logger test started");
    LOG_ERROR("This is test error message");
    LOG_DEBUG("This is test debug message: value = %d", 123);

    logger_close();

    return 0;
}