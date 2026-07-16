# PriceUpdater

## Description

PriceUpdater is a command-line utility for managing parking cities and parking prices in the Parking-System project.

The utility directly updates the SQLite database through the SQLiteDatabase library and notifies the running Database application by sending a `SIGUSR1` signal. The Database process is identified using its PID file.

---

## Features

- Command-line menu interface
- Add new parking cities
- Update parking prices
- Delete parking cities
- Direct SQLite database access
- Database notification using `SIGUSR1`
- Database process identification through a PID file
- Configuration file support
- Centralized logging through the Logger library

---

## Responsibilities

- Read user commands
- Modify parking city information
- Update parking prices
- Delete parking cities
- Read the Database process ID from the PID file
- Notify the Database application after successful modifications

---

## Architecture

```text
             User
              │
              ▼
        PriceUpdater
              │
              ▼
   SQLiteDatabase Library
              │
              ▼
       SQLite Database

              │
      Read PID File
              │
              ▼
      Database Process
              │
              ▼
         Send SIGUSR1
```

---

## Menu

```text
1. Add city
2. Update city price
3. Delete city
4. Exit
```

---

## Notification

After every successful database modification, PriceUpdater:

1. Reads the Database process ID from the PID file.
2. Sends a `SIGUSR1` signal to the running Database application.

This mechanism allows the Database application to detect parking information updates without restarting the system.

---

## Dependencies

- Config
- Logger
- SQLiteDatabase
