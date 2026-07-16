# TcpServer

## Description

TcpServer is a multi-client TCP server that acts as the communication gateway between remote BBG clients and the internal Parking-System IPC subsystem.

The server accepts parking requests from multiple TCP clients, forwards them to the Database application through shared memory queues, and returns the corresponding responses to the originating clients.

---

## Features

- Multi-client TCP server
- Event-driven client handling using `select()`
- Three dedicated worker threads
- Shared memory IPC with the Database application
- Thread-safe request processing
- Graceful shutdown
- Configuration file support
- Centralized logging through the Logger library

---

## Responsibilities

- Accept TCP client connections
- Receive parking requests
- Forward requests to Database through SharedQueue
- Receive Database responses
- Route responses back to the correct TCP client
- Maintain active client information
- Coordinate communication between the network and IPC layers

---

## Architecture

```text
                 TCP Clients
                      │
               Multiple Connections
                      │
                +-------------+
                |  select()   |
                +-------------+
                      │
        ┌─────────────┴─────────────┐
        │                           │
        ▼                           ▼
Internal Request Queue      Client Table
        │
        ▼
 Request Writer Thread
        │
        ▼
 SharedQueue (TCP → Database)
        │
        ▼
      Database
        │
        ▼
 SharedQueue (Database → TCP)
        │
        ▼
 Response Reader Thread
        │
        ▼
     TCP Clients
```

---

## Worker Threads

### TCP Server Thread

- Listens for incoming TCP connections
- Uses `select()` to monitor all client sockets
- Receives parking requests
- Places requests into the internal request queue

### Request Writer Thread

- Removes requests from the internal queue
- Sends requests to the Database application through SharedQueue

### Response Reader Thread

- Receives Database responses
- Matches responses to pending client requests
- Sends responses back to the correct TCP client

---

## IPC

TcpServer communicates with the Database application through two shared queues located inside a shared memory segment.

- **TCP → Database** — parking requests
- **Database → TCP** — processing results

The shared memory segment is created and owned by the Database application.

TcpServer attaches to the existing IPC resources during startup.

---

## Dependencies

- Config
- Logger
- SharedMemory
- SharedQueue
- IpcProtocol
- POSIX Threads
