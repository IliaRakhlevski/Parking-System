#ifndef SHARED_QUEUE_HPP
#define SHARED_QUEUE_HPP

#include "shared_memory.hpp"

#include <cstddef>
#include <semaphore.h>

/**
 * @brief Queue header stored in shared memory.
 */
struct queue_header_t
{
    std::size_t head;
    std::size_t tail;
    std::size_t count;
    std::size_t capacity;
    std::size_t element_size;
};

/**
 * @brief Shared memory FIFO queue.
 */
class SharedQueue
{
public:
    /**
     * @brief Create SharedQueue object.
     *
     * @param shared_memory Pointer to SharedMemory object.
     * @param semaphore_name POSIX named semaphore.
     * @param element_size Queue element size in bytes.
     * @param capacity Maximum number of elements.
     */
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

private:

    /**
     * @brief Pointer to SharedMemory object.
     */
    SharedMemory *shared_memory_;

    /**
     * @brief Queue offset from the beginning of shared memory.
     */
    std::size_t offset_;

    /**
     * @brief Pointer to queue header in shared memory.
     */
    queue_header_t *header_;

    /**
     * @brief Pointer to queue data in shared memory.
     */
    void *data_;

    /**
     * @brief POSIX named semaphore.
     */
    sem_t *semaphore_;

    /**
     * @brief Semaphore name.
     */
    const char *semaphore_name_;

    /**
     * @brief Queue element size in bytes.
     */
    std::size_t element_size_;

    /**
     * @brief Maximum number of queue elements.
     */
    std::size_t capacity_;
};

#endif /* SHARED_QUEUE_HPP */