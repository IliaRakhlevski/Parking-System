#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logger.h
 * @brief Thread-safe logger interface.
 */

#define LOGGER_ENABLE_DEBUG 1

/**
 * @brief Log message severity levels.
 */
typedef enum
{
    LOG_LEVEL_INFO = 0,   /**< Informational message. */
    LOG_LEVEL_ERROR,      /**< Error message. */
    LOG_LEVEL_DEBUG       /**< Debug message. */
} log_level_t;

int logger_init(const char *log_file_path, int enable_console);

void logger_close(void);

void logger_write(log_level_t level,
                  const char *file,
                  int line,
                  const char *format,
                  ...);

#define LOG_INFO(...)  logger_write(LOG_LEVEL_INFO,  NULL, 0, __VA_ARGS__)
#define LOG_ERROR(...) logger_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#if LOGGER_ENABLE_DEBUG
#define LOG_DEBUG(...) logger_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */