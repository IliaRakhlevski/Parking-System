# SharedMemory

## Description

SharedMemory is a reusable C++ library that provides an object-oriented wrapper around System V shared memory.

The library is designed for inter-process communication (IPC) and provides a simple interface for creating, attaching, detaching and removing shared memory segments.

Serves as the storage backend for the SharedQueue library.

---

## Features

- Object-oriented C++ interface
- Uses System V shared memory
- Creates new shared memory segments
- Attaches to existing shared memory segments
- Detaches shared memory segments
- Removes shared memory segments
- Provides direct access to the shared memory address
- Reports the number of attached processes
- Reports errors through the Logger library

---

## Public Interface

```cpp
SharedMemory(key_t key, std::size_t size);

~SharedMemory();

bool create();
bool attach();

bool detach();
bool remove();

void *data();
const void *data() const;

int id() const;
bool is_attached() const;
int attached_count() const;
```

---

## Lifecycle

Typical usage:

```text
Create
   │
   ▼
Attach
   │
   ▼
Access shared memory
   │
   ▼
Detach
   │
   ▼
Remove (optional)
```

The library automatically detaches from shared memory when the object is destroyed.

Removing the shared memory segment is always an explicit application decision.

---

## Notes

- The constructor only stores the shared memory parameters.
- `create()` creates a new shared memory segment and attaches it.
- `attach()` connects to an existing shared memory segment.
- The destructor automatically detaches the shared memory if it is attached.
- Shared memory is **not** removed automatically.
- `remove()` succeeds only when no other processes remain attached.

---

## Dependencies

- Logger
