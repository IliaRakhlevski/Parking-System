# TcpServer

## Description

The TcpServer application is responsible for communicating with TCP clients and exchanging messages with the Database application through the IPC subsystem.

## Current status

Implemented:

- Configuration loading.
- Logger initialization.
- Connection to an existing shared memory segment.
- Waiting for IPC resources created by Database.
- Attachment to both shared queues.
- Graceful shutdown.

Not implemented yet:

- TCP socket initialization.
- Client connection handling.
- Message exchange with Database.
- Request processing.

## IPC initialization sequence

The TcpServer application performs the following initialization sequence:

1. Load the configuration file.
2. Initialize the logger.
3. Wait for the shared memory segment created by Database.
4. Attach to the shared memory.
5. Wait until both shared queues are ready.
6. Attach to both shared queues.
7. Enter the main processing loop.

## Shutdown

The TcpServer application:

- destroys local queue objects;
- detaches from the shared memory segment;
- releases local resources;
- closes the logger.

The TcpServer application does not remove the shared memory segment because it is owned by the Database application.

## Dependencies

- Logger
- Config
- SharedMemory
- SharedQueue
- IpcProtocol

## Build

```bash
make
```

## Run

```bash
./tcp_server_test
```

## Author

Ilia
