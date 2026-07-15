## Logger

Logger is a reusable C library.

Log levels:
- INFO: always enabled.
- ERROR: always enabled.
- DEBUG: enabled or disabled at compile time.

Each project module owns its own log file:
- Server writes to its own log file.
- Database writes to its own log file.
- BBG daemon writes to its own log file.

Initial logger outputs:
- console
- file

Future possible output:
- syslog

Logger behavior:
- On module startup, logger opens its log file in write mode.
- Existing log file content is cleared.
- During runtime, all log messages are appended to the opened file.
- Logger must be thread-safe.
- A mutex protects console and file writes.


## Config Library

### Storage

Configuration entries are stored in a static array.

Reason:
- Predictable memory usage.
- No dynamic memory allocation.
- Sufficient for the expected configuration size.

### Value Types

All values are stored internally as strings.

Reason:
- Simpler parser.
- Universal storage format.
- Type conversion is performed on request.

### Responsibility

The Config library validates only data types.

Semantic validation (ranges, limits, business rules) is the responsibility of the application.


## SharedMemory Library

### Technology

The SharedMemory library uses System V shared memory API:

- `shmget()`
- `shmat()`
- `shmdt()`
- `shmctl()`

This API was selected because it was studied during the course and is sufficient for the project requirements.

### Language

SharedMemory is implemented as a C++ class because it is used by C++ modules:

- TCP Server
- Database

### Design

The class uses RAII principles:

- The constructor only stores shared memory parameters.
- `create()` creates and attaches a new shared memory segment.
- `attach()` attaches to an existing shared memory segment.
- `detach()` detaches from the segment.
- The destructor automatically calls `detach()` if needed.

The destructor does **not** remove the shared memory segment.  
Removing shared memory is an explicit application decision and must be done by calling `remove()`.

### Responsibility

The library is responsible for:

- creating shared memory;
- attaching to shared memory;
- detaching from shared memory;
- removing shared memory when explicitly requested;
- providing access to the attached memory address;
- reporting the current number of attachments.

The library is not responsible for:

- defining the data layout stored inside shared memory;
- synchronizing access to shared memory;
- deciding when the segment should be removed.

Synchronization will be handled separately.

## SharedQueue Library

### Language

The SharedQueue library is implemented in C++ because it is used by C++ applications.

### Purpose

The library provides a reusable FIFO queue for inter-process communication (IPC).

It is intended to exchange data between independent Linux processes.

### Shared Memory Layout

The queue does not own the shared memory segment.

A SharedMemory object is created separately and passed to the SharedQueue constructor.

Each queue occupies its own region inside the shared memory segment.

The queue location is determined by an offset from the beginning of shared memory.

This design allows multiple independent queues to be stored inside a single shared memory segment.

### Queue Structure

Each queue consists of:

- queue header;
- fixed-size element storage.

The queue header stores:

- head index;
- tail index;
- current number of elements;
- queue capacity;
- element size.

### Queue Elements

Queue elements have a fixed size.

The element size is specified during queue construction and remains constant during the queue lifetime.

Variable-length messages are intentionally not supported because fixed-size records simplify memory management and improve performance.

### Synchronization

Access to the queue is protected by a POSIX named semaphore.

The semaphore is used as a binary semaphore (mutex).

Only one process may modify the queue at a time.

Queue operations are non-blocking.

- push() returns false if the queue is full.
- pop() returns false if the queue is empty.

### Ownership

SharedQueue does not own the SharedMemory object.

The application is responsible for:

- creating shared memory;
- attaching shared memory;
- removing shared memory.

SharedQueue is responsible only for queue management inside the shared memory region.

### Error Handling

All internal errors are reported using the Logger library.

Library functions return status values instead of terminating the application.


## IPC Protocol

### Decision

A separate header-only module named `IpcProtocol` is introduced to store all common IPC definitions shared between the `Database` and `TcpServer` applications.

Directory structure:

```text
Libraries/
└── IpcProtocol/
    └── Inc/
        └── ipc_protocol.hpp
```

### Contents

The `ipc_protocol.hpp` header contains only shared definitions, including:

- Shared memory header structure.
- IPC request and response structures.
- Enumerations for commands and status codes.
- Shared memory layout constants.
- Queue names.
- IPC-related constants.

No implementation code is placed in this module.

### Rationale

The `Database` and `TcpServer` applications exchange data through shared memory. Keeping all common definitions in a single header provides:

- A single source of truth for the IPC protocol.
- No duplicated structures or constants.
- Guaranteed binary compatibility between applications.
- Easier maintenance when the protocol changes.
- Clear separation between IPC protocol definitions and IPC implementation.

The `SharedMemory` and `SharedQueue` libraries implement the IPC mechanisms, while `IpcProtocol` defines the format of the exchanged data.


## Database Application

The Database application is responsible for initializing and managing the IPC infrastructure of the Parking System.

### Initialization

The Database application performs the following initialization sequence:

1. Load the application configuration.
2. Initialize the logger.
3. Create and attach the System V shared memory segment.
4. Initialize the IPC header.
5. Create the TcpServer-to-Database shared queue.
6. Set the corresponding READY flag.
7. Create the Database-to-TcpServer shared queue.
8. Set the corresponding READY flag.

The application enters the main processing loop only after all IPC resources have been successfully initialized.

### Shutdown

The Database application owns the lifetime of the IPC resources.

During shutdown it performs the following operations:

1. Destroy both shared queue objects.
2. Wait until all other processes detach from the shared memory segment.
3. Detach itself from the shared memory.
4. Remove the shared memory segment.
5. Release allocated resources.
6. Close the logger.

### Configuration

The Database application loads its configuration from a common configuration file shared by all applications in the project.

Parameters that describe the IPC memory layout (queue offsets, queue sizes, shared memory size, etc.) are compile-time constants and are not configurable. Only runtime parameters, such as the shared memory key, database path and logging options, are loaded from the configuration file.


## TcpServer Application

The TcpServer application does not create IPC resources. It always connects to the IPC infrastructure previously created by the Database application.

### Shared Memory Attachment

During startup, TcpServer repeatedly attempts to attach to the existing System V shared memory segment.

To avoid busy polling, a fixed delay is inserted between attachment attempts.

The attachment process is limited by a configurable timeout. If the timeout expires before the shared memory becomes available, the initialization fails.

### Queue Synchronization

After successfully attaching to the shared memory segment, TcpServer waits until both queue readiness flags become `READY`.

The queues are attached only after both flags indicate that the Database application has completely initialized the IPC subsystem.

This prevents TcpServer from accessing partially initialized shared queues.

### Queue Ownership

TcpServer does not create or initialize shared queues.

It only attaches to the queues created by the Database application using predefined shared memory offsets.

### Shutdown

During shutdown, TcpServer:

1. destroys its local shared queue objects;
2. detaches from the shared memory segment;
3. releases local resources;
4. closes the logger.

TcpServer never removes the shared memory segment because its lifetime is managed by the Database application.

### Startup Independence

The Parking System does not require a predefined startup order.

If the Database application starts first, TcpServer attaches immediately.

If TcpServer starts first, it waits for the shared memory segment and continues initialization after the Database application becomes available.

This allows both applications to be started independently without external synchronization.



## SQLiteDatabase library

### Decision

SQLite support was moved from the `Database` module into a separate reusable library (`Libraries/SQLiteDatabase`).

### Motivation

The initial design assumed that only the `Database` process would access the SQLite database.

Later, the project architecture was extended with a standalone command-line utility for managing parking information. Since both the `Database` process and the command-line utility require access to the same database, placing all SQLite code inside the `Database` module would result in duplicated SQL logic.

### Result

A dedicated `SQLiteDatabase` library was introduced.

The library is responsible only for SQLite operations:

- opening and closing the database;
- creating the database schema;
- executing SQL statements;
- inserting, updating and retrieving parking data.

The library is independent of:

- IPC;
- Shared Memory;
- Shared Queue;
- TCP communication;
- application-specific business logic.

Both the `Database` process and the command-line utility use the same library to access the SQLite database.


## BBG Client architecture

### Decision

The BBG Client was implemented as two independent processes: an **I2C process** and a **TCP process**, communicating through an unnamed POSIX pipe.

The I2C process is responsible for receiving data from the STM32 microcontroller and generating parking events. The TCP process is responsible only for communication with the Parking System TCP server.

### Motivation

The STM32 continuously provides GPS coordinates, while communication with the Parking Server is required only when a parking session starts or ends.

Combining hardware communication and network communication into a single process would tightly couple these two independent responsibilities, making the code more difficult to maintain and extend.

Separating them into two dedicated processes provides a clear separation of concerns and allows each process to perform a single well-defined task.

### Result

The BBG Client consists of:

- an **I2C process** responsible for hardware communication and parking event generation;
- a **TCP process** responsible for TCP communication with the Parking Server;
- an unnamed POSIX pipe used for inter-process communication.

During development, the STM32 is temporarily replaced by a deterministic parking event generator running inside the I2C process.

When the STM32 firmware becomes available, only the event generator will be replaced. The overall BBG Client architecture and inter-process communication will remain unchanged.


## PriceUpdater utility

### Decision

A standalone command-line utility (`PriceUpdater`) was introduced for managing parking city information.

The utility directly updates the SQLite database and notifies the running `Database` process using the `SIGUSR1` signal.

### Motivation

The `Database` module is responsible for processing parking requests and maintaining the system state.

Administrative operations, such as adding a city, updating parking prices or deleting a city, are independent from the main parking workflow.

Moving these operations into a separate utility keeps the `Database` process focused on its primary responsibility and avoids mixing administrative functionality with runtime request processing.

### Result

The `PriceUpdater` utility is responsible for:

- adding a new parking city;
- updating parking prices;
- deleting parking cities;
- notifying the `Database` process after a successful database modification.

The utility uses the existing project libraries:

- `Config`;
- `Logger`;
- `SQLiteDatabase`.

The `Database` process identifier is obtained from a PID file whose location is defined in the common configuration file.

After every successful database modification, the utility sends the `SIGUSR1` signal to the running `Database` process.


