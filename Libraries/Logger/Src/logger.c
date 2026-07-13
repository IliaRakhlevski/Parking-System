#define _POSIX_C_SOURCE 200809L
#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/**
 * @brief Maximum size of a formatted log message.
 */
#define LOGGER_MESSAGE_MAX_SIZE    1024

/**
 * @brief Buffer size for formatted timestamp.
 */
#define LOGGER_TIME_BUFFER_SIZE    32

/**
 * @brief Log file descriptor.
 *
 * Initialized by logger_init() and released by logger_close().
 */
static int log_fd = -1;

/**
 * @brief Mutex protecting logger resources.
 *
 * Prevents simultaneous access to the console and log file
 * from multiple threads.
 */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Enables or disables console logging.
 */
static int console_output_enabled = 0;

/**
 * @brief Convert log level to a text string.
 *
 * @param level Log message level.
 *
 * @return Pointer to constant string representation.
 */
static const char *logger_level_to_string(log_level_t level)
{
    switch (level)
    {
        case LOG_LEVEL_INFO:
            return "INFO ";

        case LOG_LEVEL_ERROR:
            return "ERROR";

        case LOG_LEVEL_DEBUG:
            return "DEBUG";

        default:
            return "UNKWN";
    }
}

/**
 * @brief Generate current local timestamp.
 *
 * Format:
 * YYYY-MM-DD HH:MM:SS
 *
 * @param buffer Destination buffer.
 * @param buffer_size Destination buffer size.
 */
static void logger_get_timestamp(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm time_info;

    now = time(NULL);

    if (localtime_r(&now, &time_info) == NULL)
    {
        snprintf(buffer, buffer_size, "unknown-time");
        return;
    }

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &time_info);
}

/**
 * @brief Initialize the logger.
 *
 * Opens the specified log file in truncate mode and initializes
 * all internal logger resources.
 *
 * @param log_file_path Path to the log file.
 * @param enable_console Enable or disable console logging.
 *
 * @return
 *      - 0  Success.
 *      - -1 Initialization failed.
 */
int logger_init(const char *log_file_path, int enable_console)
{
    if (log_file_path == NULL)
    {
        return -1;
    }

    console_output_enabled = enable_console;

    pthread_mutex_lock(&log_mutex);

    if (log_fd != -1)
    {
        close(log_fd);
        log_fd = -1;
    }

    log_fd = open(log_file_path,
                    O_WRONLY | O_CREAT | O_TRUNC,
                    0644);

    pthread_mutex_unlock(&log_mutex);

    if (log_fd == -1)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief Release all logger resources.
 *
 * Closes the log file if it is open.
 */
void logger_close(void)
{
    pthread_mutex_lock(&log_mutex);

    if (log_fd != -1)
    {
        close(log_fd);
        log_fd = -1;
    }

    pthread_mutex_unlock(&log_mutex);
}

/**
 * @brief Write a formatted log message.
 *
 * The function formats the message, prepends timestamp,
 * log level and source location, then writes the message
 * to both console and log file.
 *
 * This function is thread-safe.
 *
 * @param level Log message level.
 * @param file Source file name.
 * @param line Source file line number.
 * @param format printf-style format string.
 * @param ... Variable argument list.
 */
void logger_write(log_level_t level,
                  const char *file,
                  int line,
                  const char *format,
                  ...)
{
    char time_buffer[LOGGER_TIME_BUFFER_SIZE];
    char user_message[LOGGER_MESSAGE_MAX_SIZE];
    char final_message[LOGGER_MESSAGE_MAX_SIZE];
    va_list args;
    int final_length;

    if ((format == NULL) ||
        ((log_fd == -1) && (!console_output_enabled)))
    {
        return;
    }

    logger_get_timestamp(time_buffer, sizeof(time_buffer));

    va_start(args, format);
    vsnprintf(user_message, sizeof(user_message), format, args);
    va_end(args);

    if (file != NULL)
    {
        final_length = snprintf(final_message,
                                sizeof(final_message),
                                "%s [%s] %s:%d %s\n",
                                time_buffer,
                                logger_level_to_string(level),
                                file,
                                line,
                                user_message);
    }
    else
    {
        final_length = snprintf(final_message,
                                sizeof(final_message),
                                "%s [%s] %s\n",
                                time_buffer,
                                logger_level_to_string(level),
                                user_message);
    }

    if (final_length <= 0)
    {
        return;
    }

    if (final_length >= (int)sizeof(final_message))
    {
        final_length = sizeof(final_message) - 1;
    }

    pthread_mutex_lock(&log_mutex);

    if (console_output_enabled)
    {
        int console_fd = (level == LOG_LEVEL_ERROR) ? STDERR_FILENO : STDOUT_FILENO;
        write(console_fd, final_message, final_length);
    }

    if (log_fd != -1)
    {
        write(log_fd, final_message, final_length);
    }

    pthread_mutex_unlock(&log_mutex);
}
