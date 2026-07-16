# Database

## Description

Database is the central processing component of the Parking-System project.

It owns the IPC infrastructure, manages the SQLite database, processes parking requests received from TcpServer, and returns the corresponding responses.

The Database application is the owner of the IPC subsystem. It creates and manages the shared memory segment and both shared queues during startup. All other applications attach to these IPC resources after they have been initialized.

---

## Features

- SQLite database management
- Owner of the IPC subsystem
- Creates and manages the System V shared memory segment
- Creates both shared memory queues
- Two dedicated worker threads
- Signal handling (`SIGUSR1`)
- PID file generation
- Parking request processing
- Parking cost calculation
- Graceful shutdown
- Configuration file support
- Centralized logging through the Logger library

---

## Responsibilities

- Initialize the SQLite database
- Create the database schema
- Create and own the shared memory segment
- Initialize shared IPC queues
- Receive requests from TcpServer
- Process parking operations
- Send responses back through IPC
- Handle parking database updates
- Release IPC resources during shutdown

---

## Architecture

```text
             TcpServer
                  │
                  ▼
     SharedQueue (TCP → Database)
                  │
                  ▼
        Request Reader Thread
                  │
                  ▼
       Internal Request Queue
                  │
                  ▼
      Response Writer Thread
                  │
        SQLiteDatabase Library
                  │
                  ▼
             SQLite Database
                  │
                  ▼
    SharedQueue (Database → TCP)
                  │
                  ▼
             TcpServer
```

---

## Worker Threads

### Request Reader Thread

- Receives requests from the shared request queue
- Places requests into the internal processing queue

### Response Writer Thread

- Processes parking requests
- Accesses the SQLite database
- Calculates parking results
- Sends responses back through the shared response queue

---

## IPC

Database owns the complete IPC infrastructure.

During startup it:

- Creates the System V shared memory segment
- Initializes both shared queues
- Publishes queue readiness flags

All other applications wait for these IPC resources and attach to them after initialization.

During shutdown Database:

- Waits until all other processes detach from shared memory
- Removes the shared memory segment
- Releases all IPC resources

---

## Signals

- **SIGUSR1** — sent by the PriceUpdater utility after parking data has been modified.

---

## Dependencies

- Config
- Logger
- SQLiteDatabase
- SharedMemory
- SharedQueue
- IpcProtocol
- POSIX Threads
