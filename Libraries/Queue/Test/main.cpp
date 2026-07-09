#include "shared_memory.hpp"
#include "shared_queue.hpp"
#include "logger.h"

#include <cstdio>
#include <cstring>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_KEY 5678
#define QUEUE_CAPACITY 5
#define TEST_SEM_NAME "/shared_queue_test_done"
#define QUEUE_SEM_NAME "/shared_queue_test_mutex"

struct test_record_t
{
    int id;
    char text[32];
};

static void child_process(std::size_t shm_size)
{
    logger_init("../../Logs/shared_queue_child.log", 1);

    SharedMemory shm(SHM_KEY, shm_size);

    if (!shm.attach())
    {
        LOG_ERROR("Child failed to attach shared memory");
        logger_close();
        return;
    }

    SharedQueue queue(&shm,
                      0,
                      QUEUE_SEM_NAME,
                      sizeof(test_record_t),
                      QUEUE_CAPACITY,
                      false);

    for (int i = 0; i < 3; i++)
    {
        test_record_t record;

        record.id = i + 1;
        std::snprintf(record.text, sizeof(record.text), "Message %d", i + 1);

        if (!queue.push(&record))
        {
            LOG_ERROR("Child failed to push record %d", record.id);
        }
        else
        {
            LOG_INFO("Child pushed record: id=%d text=%s", record.id, record.text);
        }
    }

    sem_t *done_sem = sem_open(TEST_SEM_NAME, 0);

    if (done_sem != SEM_FAILED)
    {
        sem_post(done_sem);
        sem_close(done_sem);
    }

    logger_close();
}

static void parent_process(std::size_t shm_size, pid_t child_pid)
{
    logger_init("../../Logs/shared_queue_parent.log", 1);

    sem_t *done_sem = sem_open(TEST_SEM_NAME, O_CREAT, 0666, 0);

    if (done_sem == SEM_FAILED)
    {
        LOG_ERROR("Parent sem_open() failed");
        logger_close();
        return;
    }

    SharedMemory shm(SHM_KEY, shm_size);

    if (!shm.create())
    {
        LOG_ERROR("Parent failed to create shared memory");
        sem_close(done_sem);
        sem_unlink(TEST_SEM_NAME);
        logger_close();
        return;
    }

    SharedQueue queue(&shm,
                      0,
                      QUEUE_SEM_NAME,
                      sizeof(test_record_t),
                      QUEUE_CAPACITY,
                      true);

    LOG_INFO("Parent waits for child");

    if (sem_wait(done_sem) == -1)
    {
        LOG_ERROR("Parent sem_wait() failed");
    }

    test_record_t record;

    while (queue.pop(&record))
    {
        LOG_INFO("Parent popped record: id=%d text=%s", record.id, record.text);
    }

    waitpid(child_pid, nullptr, 0);

    shm.remove();

    sem_close(done_sem);
    sem_unlink(TEST_SEM_NAME);
    sem_unlink(QUEUE_SEM_NAME);

    logger_close();
}

int main()
{
    std::size_t queue_size =
        sizeof(queue_header_t) +
        QUEUE_CAPACITY * sizeof(test_record_t);

    pid_t pid = fork();

    if (pid < 0)
    {
        std::printf("fork() failed\n");
        return 1;
    }

    if (pid == 0)
    {
        sleep(1);
        child_process(queue_size);
    }
    else
    {
        parent_process(queue_size, pid);
    }

    return 0;
}