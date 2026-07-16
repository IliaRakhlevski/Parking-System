# SQLiteDatabase

## Description

SQLiteDatabase is a reusable C library that provides a high-level interface for the Parking-System database.

The library encapsulates all SQLite operations and implements the database logic required by the application, including city management, parking session management, and parking cost calculation.

---

## Features

- SQLite database management
- Database schema initialization
- City management (add, update, delete)
- GPS coordinate to city lookup
- Parking session management
- Parking cost calculation
- Uses prepared statements for all SQL operations
- Validates function arguments
- Reports errors through the Logger library
- Provides a C-compatible interface

---

## Responsibilities

The library is responsible for:

- Opening and closing the SQLite database
- Creating the database schema
- Managing parking cities
- Finding a city by GPS coordinates
- Starting parking sessions
- Stopping parking sessions
- Calculating parking costs
- Retrieving city information

---

## Public Interface

```c
sqlite_database_open()
sqlite_database_close()

sqlite_database_initialize()

sqlite_database_add_city()
sqlite_database_update_city_price()
sqlite_database_delete_city()

sqlite_database_find_city()
sqlite_database_get_city()

sqlite_database_start_parking()
sqlite_database_stop_parking()
```

---

## Design Notes

The library encapsulates all SQL statements inside the implementation.

Application modules interact with the database exclusively through the public API and never execute SQL queries directly.

Parking sessions store the parking price that is active when the session starts. This guarantees that later price updates do not affect already active parking sessions.

---

## Dependencies

- SQLite3
- Logger
- Standard C Library
