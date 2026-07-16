# SharedQueue

## Description

SharedQueue is a reusable C++ library that implements a fixed-size FIFO queue directly inside a shared memory segment.

The library is designed for inter-process communication (IPC). It uses the SharedMemory library as its storage backend and synchronizes access with a POSIX named semaphore, allowing multiple processes to safely exchange data.

---

## Features

- Fixed-size FIFO queue stored entirely in shared memory
- Designed for inter-process communication (IPC)
- Uses the SharedMemory library as the storage backend
- Supports multiple queues within a single shared memory segment using offsets
- Uses a POSIX named semaphore for synchronization
- Thread-safe and process-safe queue operations
- Fixed-size queue elements
- Object-oriented C++ interface
- Reports errors through the Logger library

---

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

---

## Shared Memory Layout

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

Multiple queues can coexist within the same shared memory segment by placing each queue at a different offset.

---

## Notes

- Queue elements have a fixed size.
- The queue does not allocate memory dynamically.
- The `SharedMemory` object is owned by the application.
- The constructor creates or opens the named semaphore.
- The destructor closes the semaphore.
- The semaphore is not automatically removed from the system.
- Queue operations return `false` if the queue is full or empty.

---

## Dependencies

- Logger
- SharedMemory
