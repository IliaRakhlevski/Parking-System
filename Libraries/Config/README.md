# Config

## Description

Config is a reusable C library for loading and accessing application configuration files.

## Features

- Parses configuration files in KEY=VALUE format.
- Supports comments and empty lines.
- Stores configuration entries in memory.
- Retrieves values as different data types.
- Supports user-defined default values.
- Uses the Logger library to report configuration errors.

## Configuration file example

```text
SERVER_PORT=5555
DB_PATH=Database/parking.db
LOG_FILE=Logs/server.log
```

## Public Interface

```c
config_load(...);
config_get_int(...);
config_get_string(...);
...
```
