# Logger

## Description

Logger is a reusable thread-safe C library for writing formatted log messages to a file and, optionally, to the console.

The library is used throughout the Parking-System project to provide consistent logging across all modules.

---

## Features

- Thread-safe logging using POSIX mutexes
- Supports INFO, ERROR and DEBUG log levels
- Writes log messages to a file
- Optional console output
- Creates a new log file on each application start
- Supports `printf`-style formatting
- Automatically prepends timestamps to every message
- Includes source file and line number for ERROR and DEBUG messages

---

## Log Format

Example log entries:

```text
2026-07-16 12:15:42 [INFO ] TCP Server started
2026-07-16 12:15:43 [ERROR] database.c:127 Failed to open database
2026-07-16 12:15:44 [DEBUG] server.c:89 Client connected
```

---

## Public Interface

```c
int logger_init(const char *log_file_path, int enable_console);

void logger_close(void);

LOG_INFO(...);
LOG_ERROR(...);
LOG_DEBUG(...);
```

---

## Log Levels

- `LOG_INFO` — General application events.
- `LOG_ERROR` — Error messages including source file and line number.
- `LOG_DEBUG` — Debug messages including source file and line number. Enabled or disabled at compile time.

---

## Notes

- Each process should initialize its own logger instance.
- Every module should use its own log file.
- Console output can be enabled or disabled during initialization.
