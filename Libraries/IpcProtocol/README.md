# IpcProtocol

## Description

IpcProtocol defines the communication protocol shared by the TcpServer and Database applications.

It contains the common data structures, message formats and shared memory layout required for binary-compatible inter-process communication.

---

## Components

### parking_protocol.h

Defines the messages exchanged between TcpServer and Database, including:

- request and response structures;
- parking actions;
- status codes;
- common protocol constants.

### ipc_protocol.hpp

Defines the IPC infrastructure, including:

- shared memory layout;
- queue offsets;
- queue capacities;
- semaphore names;
- IPC header structure;
- readiness flags.

---

## Design

The protocol definitions are separated from the IPC implementation.

SharedMemory and SharedQueue implement the transport mechanism, while IpcProtocol defines the format of the exchanged data.

This separation guarantees that both applications use the same binary protocol without duplicating definitions.
