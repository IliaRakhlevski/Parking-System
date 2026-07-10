#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

#include <cstddef>
#include <sys/types.h>

/**
 * @brief System V shared memory wrapper.
 */
class SharedMemory
{
public:
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

private:
    /**
    * @brief Shared memory key.
    */
    key_t key_;

    /**
     * @brief Shared memory segment size in bytes.
     */
    std::size_t size_;

    /**
     * @brief Shared memory identifier returned by shmget().
     */
    int shm_id_;

    /**
     * @brief Pointer to the attached shared memory segment.
     *
     * The pointer is valid only after a successful call to create()
     * or attach().
     */
    void *addr_;
};

#endif /* SHARED_MEMORY_HPP */
