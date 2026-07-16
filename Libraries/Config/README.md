# Config

## Description

Config is a reusable C library for loading application settings from configuration files and accessing them by key.

The library is used by Parking-System modules to keep runtime parameters outside the source code.

---

## Features

- Parses configuration files in `KEY=VALUE` format
- Ignores empty lines
- Supports comments beginning with `#` or `;`
- Removes leading and trailing whitespace
- Stores configuration entries in memory
- Retrieves values as strings or integers
- Supports user-defined default values
- Reports configuration errors through the Logger library
- Provides a C-compatible interface for both C and C++ modules

---

## Configuration File Format

Each configuration entry is stored on a separate line:

```text
KEY=VALUE
```

Whitespace around keys and values is ignored.

Empty lines and lines beginning with `#` or `;` are treated as comments.

### Example

```text
# TCP server configuration
SERVER_PORT=5555

# Database configuration
DB_PATH=Database/parking.db

; Logging configuration
LOG_FILE=Logs/server.log
```

---

## Public Interface

```c
int config_load(config_t *config, const char *file_path);

const char *config_get_string(const config_t *config,
                              const char *key,
                              const char *default_value);

int config_get_int(const config_t *config,
                   const char *key,
                   int default_value);
```

### `config_load()`

Loads configuration entries from the specified file into a `config_t` object.

Returns `0` on success and `-1` if the configuration file cannot be loaded.

Invalid configuration lines are reported through the Logger library and skipped.

### `config_get_string()`

Returns the string value associated with the specified key.

If the key is not found, the supplied default value is returned.

### `config_get_int()`

Converts the value associated with the specified key to an integer.

If the key is not found or the value cannot be converted, the supplied default value is returned.

---

## Limits

- Maximum number of entries: `100`
- Maximum key length: `63` characters
- Maximum value length: `255` characters

---

## Dependencies

- Logger
- C standard library
