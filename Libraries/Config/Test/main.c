#include "config.h"
#include "logger.h"

#include <stdio.h>

int main(void)
{
    config_t config;
    const char *db_path;
    const char *log_file;
    const char *missing_string;
    int server_port;
    int missing_int;

    if (logger_init("../../Logs/config_test.log", 1) != 0)
    {
        printf("Failed to initialize logger\n");
        return 1;
    }

    if (config_load(&config, "Test/test.conf") != 0)
    {
        LOG_ERROR("Failed to load config file");
        logger_close();
        return 1;
    }

    server_port = config_get_int(&config, "SERVER_PORT", 5555);

    db_path = config_get_string(&config,
                                "DB_PATH",
                                "Database/default.db");

    log_file = config_get_string(&config,
                                 "LOG_FILE",
                                 "Logs/default.log");

    missing_string = config_get_string(&config,
                                       "MISSING_STRING",
                                       "default_string");

    missing_int = config_get_int(&config,
                                 "MISSING_INT",
                                 1234);

    LOG_INFO("SERVER_PORT = %d", server_port);
    LOG_INFO("DB_PATH = %s", db_path);
    LOG_INFO("LOG_FILE = %s", log_file);
    LOG_INFO("MISSING_STRING = %s", missing_string);
    LOG_INFO("MISSING_INT = %d", missing_int);

    logger_close();

    return 0;
}