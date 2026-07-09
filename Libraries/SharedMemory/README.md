# SharedMemory

## Description

SharedMemory is a reusable C++ library that provides a simple wrapper around System V shared memory.

The library is designed for inter-process communication (IPC) between Linux applications.

## Features

- Object-oriented C++ interface.
- Uses System V shared memory.
- Creates shared memory segments.
- Attaches to existing shared memory segments.
- Detaches shared memory.
- Removes shared memory segments.
- Provides access to the shared memory address.
- Reports the number of attached processes.
- Uses the Logger library to report errors.

## Public Interface

```cpp
SharedMemory(key_t key, std::size_t size);
~SharedMemory();

bool create();
bool attach();

void detach();
bool remove();

void *data();
const void *data() const;

int id() const;
bool is_attached() const;
int attached_count() const;
```

## Notes

- The destructor automatically detaches shared memory if it is attached.
- Shared memory is **not** automatically removed.
- The application is responsible for deciding when to remove the shared memory segment.

## Dependencies

- Logger

## Author

Ilia