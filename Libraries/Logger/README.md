# Logger

## Description

Logger is a reusable thread-safe C library designed for Linux applications.

## Features

- Thread-safe.
- Supports INFO, ERROR and DEBUG log levels.
- Writes log messages to a file.
- Optional console output.
- Creates a new log file on each application start.
- Supports formatted messages (printf style).

## Public Interface

```c
int logger_init(const char *log_file_path, int enable_console);

void logger_close(void);

LOG_INFO(...);
LOG_ERROR(...);
LOG_DEBUG(...);
```

## Notes

- Each process should initialize its own logger instance.
- Every module should use its own log file.
- DEBUG messages are enabled or disabled at compile time.

## Author

Ilia