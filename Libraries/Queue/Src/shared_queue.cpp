#include "shared_queue.hpp"
#include "logger.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <cstring>
#include <semaphore.h>


/**
 * @brief Create a SharedQueue object.
 *
 * The constructor initializes the queue using an existing
 * SharedMemory object and opens a POSIX named semaphore.
 *
 * If create is true, the queue header is initialized.
 * Otherwise, the constructor verifies that the existing queue
 * configuration matches the specified element size and capacity.
 *
 * @param shared_memory Pointer to the SharedMemory object.
 * @param offset Queue offset from the beginning of shared memory.
 * @param semaphore_name POSIX named semaphore name.
 * @param element_size Queue element size in bytes.
 * @param capacity Maximum number of queue elements.
 * @param create
 *        - true  Create and initialize the queue.
 *        - false Attach to an existing queue.
 */
SharedQueue::SharedQueue(SharedMemory *shared_memory,
                         std::size_t offset,
                         const char *semaphore_name,
                         std::size_t element_size,
                         std::size_t capacity,
                         bool create)
    : shared_memory_(shared_memory),
      offset_(offset),
      header_(nullptr),
      data_(nullptr),
      semaphore_(SEM_FAILED),
      semaphore_name_(semaphore_name),
      element_size_(element_size),
      capacity_(capacity)
{
    char *base_address;

    if ((shared_memory_ == nullptr) ||
        (shared_memory_->data() == nullptr) ||
        (semaphore_name_ == nullptr) ||
        (element_size_ == 0) ||
        (capacity_ == 0))
    {
        LOG_ERROR("Invalid SharedQueue constructor argument");
        return;
    }

    semaphore_ = sem_open(semaphore_name_,
                          O_CREAT,
                          0666,
                          1);

    if (semaphore_ == SEM_FAILED)
    {
        LOG_ERROR("sem_open() failed: %s", strerror(errno));
        return;
    }

    /* Get the beginning of the shared memory segment. */
    base_address = static_cast<char *>(shared_memory_->data());

    /* Move to the beginning of this queue using the specified offset. */
    header_ = reinterpret_cast<queue_header_t *>(base_address + offset_);

    /* Queue data starts immediately after the queue header. */
    data_ = base_address + offset_ + sizeof(queue_header_t);

    if (create)
    {
        header_->head = 0;
        header_->tail = 0;
        header_->count = 0;
        header_->capacity = capacity_;
        header_->element_size = element_size_;
    }
    else
    {
        if ((header_->capacity != capacity_) ||
            (header_->element_size != element_size_))
        {
            LOG_ERROR("SharedQueue configuration mismatch");
            sem_close(semaphore_);
            semaphore_ = SEM_FAILED;
            header_ = nullptr;
            data_ = nullptr;
        }
    }
}

/**
 * @brief Check whether the shared queue was initialized successfully.
 *
 * @return
 *      - true  Queue is ready for use.
 *      - false Queue initialization failed.
 */
bool SharedQueue::is_valid() const
{
    return (header_ != nullptr) &&
           (data_ != nullptr) &&
           (semaphore_ != SEM_FAILED);
}


/**
 * @brief Destroy SharedQueue object.
 *
 * Closes the named semaphore associated with the queue.
 * The semaphore is not removed from the system.
 * Shared memory is managed by the SharedMemory object.
 */
SharedQueue::~SharedQueue()
{
    if (semaphore_ != SEM_FAILED)
    {
        sem_close(semaphore_);
    }
}

/**
 * @brief Add an element to the queue.
 *
 * The function adds a new element to the end of the queue.
 * Access to the shared memory is protected by a named semaphore.
 *
 * @param item Pointer to the element to be added.
 *
 * @return
 *         - true  Element successfully added.
 *         - false Queue is full or an error occurred.
 */
bool SharedQueue::push(const void *item)
{
    char *base;

    if ((item == nullptr) ||
        (header_ == nullptr) ||
        (data_ == nullptr) ||
        (semaphore_ == SEM_FAILED))
    {
        LOG_ERROR("Invalid push operation");
        return false;
    }

    if (sem_wait(semaphore_) == -1)
    {
        LOG_ERROR("sem_wait() failed: %s", std::strerror(errno));
        return false;
    }

    if (header_->count >= header_->capacity)
    {
        if (sem_post(semaphore_) == -1)
        {
            LOG_ERROR("sem_post() failed: %s", std::strerror(errno));
            return false;
        }
        return false;
    }

    base = static_cast<char *>(data_);

    std::memcpy(base + (header_->tail * header_->element_size),
                item,
                header_->element_size);

    header_->tail = (header_->tail + 1) % header_->capacity;
    header_->count++;

    if (sem_post(semaphore_) == -1)
    {
        LOG_ERROR("sem_post() failed: %s", std::strerror(errno));
        return false;
    }

    return true;
}

/**
 * @brief Remove an element from the queue.
 *
 * The function removes the oldest element from the queue.
 * Access to the shared memory is protected by a named semaphore.
 *
 * @param item Pointer to the destination buffer.
 *
 * @return
 *         - true  Element successfully removed.
 *         - false Queue is empty or an error occurred.
 */
bool SharedQueue::pop(void *item)
{
    char *base;

    if ((item == nullptr) ||
        (header_ == nullptr) ||
        (data_ == nullptr) ||
        (semaphore_ == SEM_FAILED))
    {
        LOG_ERROR("Invalid pop operation");
        return false;
    }

    if (sem_wait(semaphore_) == -1)
    {
        LOG_ERROR("sem_wait() failed: %s", std::strerror(errno));
        return false;
    }

    if (header_->count == 0)
    {
        if (sem_post(semaphore_) == -1)
        {
            LOG_ERROR("sem_post() failed: %s", std::strerror(errno));
            return false;
        }
        return false;
    }

    base = static_cast<char *>(data_);

    std::memcpy(item,
                base + (header_->head * header_->element_size),
                header_->element_size);

    header_->head = (header_->head + 1) % header_->capacity;
    header_->count--;

    if (sem_post(semaphore_) == -1)
    {
        LOG_ERROR("sem_post() failed: %s", std::strerror(errno));
        return false;
    }

    return true;
}

/**
 * @brief Check whether the queue is empty.
 *
 * @return
 *         - true  Queue is empty.
 *         - false Queue contains elements.
 */
bool SharedQueue::is_empty() const
{
    if (header_ == nullptr)
    {
        return true;
    }

    return (header_->count == 0);
}

/**
 * @brief Check whether the queue is full.
 *
 * @return
 *         - true  Queue is full.
 *         - false Queue has free space.
 */
bool SharedQueue::is_full() const
{
    if (header_ == nullptr)
    {
        return false;
    }

    return (header_->count >= header_->capacity);
}

/**
 * @brief Get the current number of elements in the queue.
 *
 * @return Number of elements currently stored in the queue.
 */
std::size_t SharedQueue::size() const
{
    if (header_ == nullptr)
    {
        return 0;
    }

    return header_->count;
}
