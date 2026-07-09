#include "shared_memory.hpp"
#include "logger.h"

#include <cerrno>
#include <cstring>
#include <sys/ipc.h>
#include <sys/shm.h>

/**
 * @brief Create SharedMemory object.
 *
 * Constructor only stores shared memory parameters.
 * It does not create or attach shared memory.
 *
 * @param key Shared memory key.
 * @param size Shared memory size in bytes.
 */
SharedMemory::SharedMemory(key_t key, std::size_t size)
    : key_(key),
      size_(size),
      shm_id_(-1),
      addr_(nullptr)
{
}

/**
 * @brief Destroy SharedMemory object.
 *
 * Automatically detaches shared memory if it is attached.
 * It does not remove the shared memory segment.
 */
SharedMemory::~SharedMemory()
{
    detach();
}

/**
 * @brief Create and attach shared memory segment.
 *
 * Uses IPC_CREAT and IPC_EXCL to ensure that a new shared memory
 * segment is created. If the segment already exists, the function fails.
 *
 * @return true on success, false on failure.
 */
bool SharedMemory::create()
{
    if (is_attached())
    {
        LOG_ERROR("Shared memory is already attached");
        return false;
    }

    shm_id_ = shmget(key_, size_, IPC_CREAT | IPC_EXCL | 0666);

    if (shm_id_ == -1)
    {
        LOG_ERROR("shmget() create failed: %s", std::strerror(errno));
        return false;
    }

    addr_ = shmat(shm_id_, nullptr, 0);

    if (addr_ == (void *)-1)
    {
        LOG_ERROR("shmat() after create failed: %s", std::strerror(errno));
        addr_ = nullptr;
        return false;
    }

    return true;
}

/**
 * @brief Attach to an existing shared memory segment.
 *
 * The segment must already exist.
 *
 * @return true on success, false on failure.
 */
bool SharedMemory::attach()
{
    if (is_attached())
    {
        LOG_ERROR("Shared memory is already attached");
        return false;
    }

    shm_id_ = shmget(key_, size_, 0666);

    if (shm_id_ == -1)
    {
        LOG_ERROR("shmget() attach failed: %s", std::strerror(errno));
        return false;
    }

    addr_ = shmat(shm_id_, nullptr, 0);

    if (addr_ == (void *)-1)
    {
        LOG_ERROR("shmat() attach failed: %s", std::strerror(errno));
        addr_ = nullptr;
        return false;
    }

    return true;
}

/**
 * @brief Detach shared memory segment.
 */
void SharedMemory::detach()
{
    if (addr_ != nullptr)
    {
        if (shmdt(addr_) == -1)
        {
            LOG_ERROR("shmdt() failed: %s", std::strerror(errno));
        }

        addr_ = nullptr;
    }
}

/**
 * @brief Remove shared memory segment.
 *
 * Removes the segment from the system using IPC_RMID.
 *
 * @return true on success, false on failure.
 */
bool SharedMemory::remove()
{
    if (shm_id_ == -1)
    {
        LOG_ERROR("Cannot remove shared memory: invalid shm_id");
        return false;
    }

    int count = attached_count();

    if (count < 0)
    {
        return false;
    }

    if (count > 1)
    {
        LOG_ERROR("Cannot remove shared memory: %d processes are still attached", count);
        return false;
    }

    if (shmctl(shm_id_, IPC_RMID, nullptr) == -1)
    {
        LOG_ERROR("shmctl() IPC_RMID failed: %s", std::strerror(errno));
        return false;
    }

    shm_id_ = -1;

    return true;
}

/**
 * @brief Get attached shared memory address.
 *
 * @return Pointer to shared memory or nullptr if not attached.
 */
void *SharedMemory::data()
{
    return addr_;
}

/**
 * @brief Get attached shared memory address.
 *
 * @return Constant pointer to shared memory or nullptr if not attached.
 */
const void *SharedMemory::data() const
{
    return addr_;
}

/**
 * @brief Get shared memory identifier.
 *
 * @return Shared memory identifier or -1 if not created/attached.
 */
int SharedMemory::id() const
{
    return shm_id_;
}

/**
 * @brief Check whether shared memory is attached.
 *
 * @return true if attached, false otherwise.
 */
bool SharedMemory::is_attached() const
{
    return (addr_ != nullptr);
}

/**
 * @brief Get number of current shared memory attachments.
 *
 * @return Number of attachments, or -1 on failure.
 */
int SharedMemory::attached_count() const
{
    struct shmid_ds shm_info;

    if (shm_id_ == -1)
    {
        LOG_ERROR("Cannot get attached count: invalid shm_id");
        return -1;
    }

    if (shmctl(shm_id_, IPC_STAT, &shm_info) == -1)
    {
        LOG_ERROR("shmctl() IPC_STAT failed: %s", std::strerror(errno));
        return -1;
    }

    return static_cast<int>(shm_info.shm_nattch);
}