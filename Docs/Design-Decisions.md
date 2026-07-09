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


