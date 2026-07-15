# PriceUpdater

## Description

PriceUpdater is a simple command-line utility for the Parking-System project.

Its responsibilities are:

- add a new parking city;
- update the parking price of an existing city;
- delete a parking city;
- notify the Database module after a successful modification.

The utility directly updates the SQLite database and then sends the `SIGUSR1` signal to the running Database process.

---

## Features

- Console menu interface
- SQLite database support
- Shared project configuration
- Logger integration
- Database notification using `SIGUSR1`
- PID file based process identification

---

## Dependencies

- Config
- Logger
- SQLiteDatabase

---

## Build

```bash
make
```

---

## Run

```bash
./price_updater
```

---

## Menu

```text
1. Add city
2. Update city price
3. Delete city
4. Exit
```
