#include <chrono>
#include <thread>
#include "database.hpp"
#include "logger.h"
#include "config.h"


/**
 * @brief Construct a Database application object.
 *
 * Initializes all internal pointers to nullptr.
 * Actual application initialization is performed
 * by the initialize() function.
 */
Database::Database()
    : shared_memory_(nullptr),
      tcp_to_database_queue_(nullptr),
      database_to_tcp_queue_(nullptr),
      ipc_header_(nullptr),
      shared_memory_key_(IPC_SHARED_MEMORY_KEY),
      shared_memory_size_(IPC_SHARED_MEMORY_SIZE),
      database_file_(DEFAULT_DATABASE_FILE),
      log_file_(DEFAULT_DATABASE_LOG_FILE),
      enable_console_logging_(DEFAULT_ENABLE_CONSOLE_LOGGING)
{
}


/**
 * @brief Destroy the Database application object.
 *
 * Releases all allocated resources and shuts down
 * the logger subsystem.
 */
Database::~Database()
{
    logger_close();
}

/**
 * @brief Initialize the Database application.
 *
 * Performs application startup:
 * - initializes the logger;
 * - loads the configuration file;
 * - prepares the application for further initialization.
 *
 * @return
 *      - true  Initialization completed successfully.
 *      - false Initialization failed.
 */
bool Database::initialize()
{
    if (!load_configuration())
    {
        return false;
    }

    if (logger_init(log_file_.c_str(), 1) != 0)
    {
        return false;
    }

    LOG_INFO("Logger initialized.");

    LOG_INFO("Configuration loaded.");

    if (!initialize_shared_memory())
    {
        return false;
    }

    if (!initialize_queues())
    {
        return false;
    }

    LOG_INFO("Database IPC initialized successfully.");

    return true;
}

/**
 * @brief Shut down the Database application.
 *
 * Releases the shared queue objects, waits until all other processes
 * detach from the shared memory segment, detaches the Database process,
 * removes the shared memory segment and releases local resources.
 *
 * @return
 *      - true  Shutdown completed successfully.
 *      - false An error occurred during shutdown.
 */
bool Database::shutdown()
{
    int attached_processes;

    if (ipc_header_ != nullptr)
    {
        ipc_header_->database_to_tcp_queue_ready = ipc_ready_state_t::NOT_READY;

        ipc_header_->tcp_to_database_queue_ready = ipc_ready_state_t::NOT_READY;
    }

    ipc_header_ = nullptr;

    delete database_to_tcp_queue_;
    database_to_tcp_queue_ = nullptr;

    delete tcp_to_database_queue_;
    tcp_to_database_queue_ = nullptr;

    if (shared_memory_ == nullptr)
    {
        LOG_INFO("Database shutdown completed.");

        logger_close();

        return true;
    }

    attached_processes = shared_memory_->attached_count();

    while (attached_processes > 1)
    {
        LOG_INFO("Waiting for other processes to detach from shared memory.");

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));

        attached_processes = shared_memory_->attached_count();
    }

    if (attached_processes == -1)
    {
        LOG_ERROR("Failed to get shared memory attachment count.");

        return false;
    }

    if (shared_memory_->is_attached())
    {
        if (!shared_memory_->detach())
        {
            LOG_ERROR("Failed to detach Database from shared memory.");

            return false;
        }
    }

    if (!shared_memory_->remove())
    {
        LOG_ERROR("Failed to remove shared memory segment.");

        return false;
    }

    delete shared_memory_;
    shared_memory_ = nullptr;

    LOG_INFO("Database shutdown completed.");

    return true;
}

/**
 * @brief Load the application configuration.
 *
 * Reads the configuration file and stores all
 * required parameters for further initialization.
 *
 * @return
 *      - true  Configuration loaded successfully.
 *      - false Failed to load the configuration file.
 */
bool Database::load_configuration()
{
    config_t config;

    if (config_load(&config, DEFAULT_CONFIG_FILE) != 0)
    {
        LOG_ERROR("Failed to load configuration file.");
        return false;
    }

    shared_memory_key_ = config_get_int(&config, "IPC_SHARED_MEMORY_KEY", IPC_SHARED_MEMORY_KEY);                   
          
    database_file_ = config_get_string(&config, "DATABASE_FILE", DEFAULT_DATABASE_FILE);

    log_file_ = config_get_string(&config, "LOG_FILE", DEFAULT_DATABASE_LOG_FILE);

    enable_console_logging_ = config_get_int(&config, "ENABLE_CONSOLE_LOGGING", DEFAULT_ENABLE_CONSOLE_LOGGING);

    return true;
}

/**
 * @brief Create and attach the shared memory segment.
 *
 * Creates the System V shared memory segment using the configured key
 * and the size defined by the IPC protocol. The shared memory header
 * is initialized with both queue readiness flags set to NOT_READY.
 *
 * @return
 *      - true  Shared memory initialized successfully.
 *      - false Shared memory initialization failed.
 */
bool Database::initialize_shared_memory()
{
    shared_memory_ = new SharedMemory(shared_memory_key_, IPC_SHARED_MEMORY_SIZE);

    if (!shared_memory_->create())
    {
        LOG_ERROR("Failed to create shared memory");

        delete shared_memory_;
        shared_memory_ = nullptr;

        return false;
    }

    ipc_header_ = static_cast<ipc_header_t *>(shared_memory_->data());

    if (ipc_header_ == nullptr)
    {
        LOG_ERROR("Failed to get shared memory address");

        delete shared_memory_;
        shared_memory_ = nullptr;

        return false;
    }

    ipc_header_->tcp_to_database_queue_ready = ipc_ready_state_t::NOT_READY;

    ipc_header_->database_to_tcp_queue_ready = ipc_ready_state_t::NOT_READY;

    LOG_INFO("Shared memory initialized.");

    return true;
}

/**
 * @brief Create and initialize the shared queues.
 *
 * Creates the TcpServer-to-Database and Database-to-TcpServer
 * queues inside the shared memory segment.
 *
 * Each readiness flag is set only after the corresponding queue
 * has been initialized successfully.
 *
 * @return
 *      - true  Both shared queues initialized successfully.
 *      - false Failed to initialize one of the queues.
 */
bool Database::initialize_queues()
{
    if ((shared_memory_ == nullptr) ||
        (!shared_memory_->is_attached()) ||
        (ipc_header_ == nullptr))
    {
        LOG_ERROR("Shared memory is not ready for queue initialization.");

        return false;
    }

    tcp_to_database_queue_ = new SharedQueue(shared_memory_,
                                    TCP_TO_DATABASE_QUEUE_OFFSET,
                                    TCP_TO_DATABASE_SEMAPHORE_NAME,
                                    sizeof(tcp_to_database_message_t),
                                    TCP_TO_DATABASE_QUEUE_CAPACITY,
                                    true);

    if (!tcp_to_database_queue_->is_valid())
    {
        LOG_ERROR("Failed to initialize TcpServer-to-Database queue.");

        delete tcp_to_database_queue_;
        tcp_to_database_queue_ = nullptr;

        return false;
    }

    ipc_header_->tcp_to_database_queue_ready = ipc_ready_state_t::READY;

    LOG_INFO("TcpServer-to-Database queue initialized.");

    database_to_tcp_queue_ = new SharedQueue(shared_memory_,
                                    DATABASE_TO_TCP_QUEUE_OFFSET,
                                    DATABASE_TO_TCP_SEMAPHORE_NAME,
                                    sizeof(database_to_tcp_message_t),
                                    DATABASE_TO_TCP_QUEUE_CAPACITY,
                                    true);

    if (!database_to_tcp_queue_->is_valid())
    {
        LOG_ERROR("Failed to initialize Database-to-TcpServer queue.");

        ipc_header_->tcp_to_database_queue_ready = ipc_ready_state_t::NOT_READY;

        delete database_to_tcp_queue_;
        database_to_tcp_queue_ = nullptr;

        delete tcp_to_database_queue_;
        tcp_to_database_queue_ = nullptr;

        return false;
    }

    ipc_header_->database_to_tcp_queue_ready = ipc_ready_state_t::READY;

    LOG_INFO("Database-to-TcpServer queue initialized.");

    return true;
}