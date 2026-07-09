# SharedQueue

## Description

SharedQueue is a reusable C++ library that implements a FIFO queue in shared memory.

The library is designed for inter-process communication (IPC) between Linux applications. It uses the SharedMemory library to access shared memory and a POSIX named semaphore to synchronize access between processes.

## Features

- Object-oriented C++ interface.
- FIFO queue implementation.
- Uses shared memory for data storage.
- Supports multiple queues in a single shared memory segment using offsets.
- Uses a POSIX named semaphore for synchronization.
- Fixed-size queue elements.
- Thread-safe and process-safe queue operations.
- Uses the Logger library to report errors.

## Public Interface

```cpp
SharedQueue(SharedMemory *shared_memory,
            std::size_t offset,
            const char *semaphore_name,
            std::size_t element_size,
            std::size_t capacity,
            bool create);

~SharedQueue();

bool push(const void *item);

bool pop(void *item);

bool is_empty() const;

bool is_full() const;

std::size_t size() const;
```

## Queue Layout

```text
+-----------------------------+
| queue_header_t              |
+-----------------------------+
| Element 0                   |
+-----------------------------+
| Element 1                   |
+-----------------------------+
| ...                         |
+-----------------------------+
| Element N                   |
+-----------------------------+
```

Multiple queues can be placed inside a single shared memory segment by specifying different offsets.

## Notes

- Queue elements have a fixed size.
- The queue does not allocate memory dynamically.
- The SharedMemory object is owned by the application.
- The constructor opens or creates the named semaphore.
- The destructor closes the semaphore.
- The semaphore is not automatically removed from the system.
- Queue operations return `false` if the queue is full or empty.

## Dependencies

- Logger
- SharedMemory

## Author

Ilia