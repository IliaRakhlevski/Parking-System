#include "shared_memory.hpp"
#include "logger.h"

#include <cstdio>
#include <cstring>

int main()
{
    SharedMemory shm(1234, 1024);

    if (logger_init("../../Logs/shared_memory_test.log", 1) != 0)
    {
        std::printf("Failed to initialize logger\n");
        return 1;
    }

    if (!shm.create())
    {
        LOG_ERROR("Failed to create shared memory");
        logger_close();
        return 1;
    }

    LOG_INFO("Shared memory created");
    LOG_INFO("Shared memory ID: %d", shm.id());

    char *buffer = static_cast<char *>(shm.data());

    std::strcpy(buffer, "Hello Shared Memory!");

    LOG_INFO("Written: %s", buffer);

    LOG_INFO("Attached processes: %d", shm.attached_count());

    shm.detach();

    LOG_INFO("Shared memory detached");

    if (!shm.remove())
    {
        LOG_ERROR("Failed to remove shared memory");
    }
    else
    {
        LOG_INFO("Shared memory removed");
    }

    logger_close();

    return 0;
}