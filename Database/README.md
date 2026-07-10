# Database

## Description

The Database application is responsible for:

- initializing the IPC infrastructure;
- creating the System V shared memory segment;
- creating both shared queues;
- managing the lifetime of shared memory;
- processing parking requests (to be implemented).

## Current status

Implemented:

- Logger initialization;
- Configuration loading;
- Shared memory creation;
- Shared queue initialization;
- IPC readiness flags initialization;
- Shared memory cleanup during shutdown.

Not implemented yet:

- SQLite database;
- Main processing loop;
- Message handling;
- Parking records management.

## Dependencies

- Logger
- Config
- SharedMemory
- SharedQueue
- IpcProtocol

## Build

```bash
make

## Author

Ilia
