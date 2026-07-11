# SQLiteDatabase

## Description

The SQLiteDatabase library provides a simple interface for working with an SQLite database.

The library is responsible for:

- opening and closing the database;
- creating the database schema;
- executing SQL queries;
- adding, updating and retrieving parking data.

The library is independent of the Database and TcpServer modules and can be used by multiple processes.

## Directory structure

SQLiteDatabase/
├── Inc
├── Src
├── Test
├── Makefile
└── README.md


## Dependencies

- SQLite3
- Standard C Library

## Language

C (C17)

## Author

Ilia
