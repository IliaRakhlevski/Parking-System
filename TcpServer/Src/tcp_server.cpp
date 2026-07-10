#include "tcp_server.hpp"
#include <chrono>
#include <thread>


/**
 * @brief Construct a TcpServer application object.
 *
 * Initializes IPC object pointers to nullptr and assigns default
 * configuration values. Actual application initialization is
 * performed by initialize().
 */
TcpServer::TcpServer()
    : shared_memory_(nullptr),
      tcp_to_database_queue_(nullptr),
      database_to_tcp_queue_(nullptr),
      ipc_header_(nullptr),
      shared_memory_key_(IPC_SHARED_MEMORY_KEY),
      log_file_(DEFAULT_TCP_SERVER_LOG_FILE),
      enable_console_logging_(DEFAULT_ENABLE_CONSOLE_LOGGING)
{
}

/**
 * @brief Destroy the TcpServer application object.
 *
 * Closes the logger. IPC resources must be released by shutdown()
 * before the object is destroyed.
 */
TcpServer::~TcpServer()
{
    logger_close();
}

/**
 * @brief Load the TcpServer application configuration.
 *
 * Reads the common Parking System configuration file and stores
 * the parameters required by the TcpServer application.
 *
 * Default values remain in use when corresponding parameters
 * are not present in the configuration file.
 *
 * @return
 *      - true  Configuration loaded successfully.
 *      - false Failed to load the configuration file.
 */
bool TcpServer::load_configuration()
{
    config_t config;

    if (config_load(&config, DEFAULT_CONFIG_FILE) != 0)
    {
        return false;
    }

    shared_memory_key_ = static_cast<key_t>( config_get_int(&config,
                           "IPC_SHARED_MEMORY_KEY",
                           IPC_SHARED_MEMORY_KEY));

    log_file_ = config_get_string(&config,
                            "TCP_SERVER_LOG_FILE",
                            DEFAULT_TCP_SERVER_LOG_FILE);

    enable_console_logging_ = config_get_int(&config,
                            "ENABLE_CONSOLE_LOGGING",
                            DEFAULT_ENABLE_CONSOLE_LOGGING);

    return true;
}


/**
 * @brief Initialize the TcpServer application.
 *
 * Performs the application startup sequence:
 * - loads the configuration file;
 * - initializes the logger;
 * - attaches to the shared memory segment;
 * - waits until both IPC queues are ready;
 * - attaches to both shared queues.
 *
 * @return
 *      - true  Initialization completed successfully.
 *      - false Initialization failed.
 */
bool TcpServer::initialize()
{
    if (!load_configuration())
    {
        return false;
    }

    if (logger_init(log_file_.c_str(), enable_console_logging_) != 0)
    {
        return false;
    }

    LOG_INFO("Logger initialized.");

    LOG_INFO("Configuration loaded.");

    if (!attach_shared_memory())
    {
        return false;
    }

    if (!wait_for_queues())
    {
        return false;
    }

    if (!attach_queues())
    {
        return false;
    }

    LOG_INFO("TcpServer IPC initialized successfully.");

    return true;
}

/**
 * @brief Attach to the shared memory segment created by Database.
 *
 * Creates a local SharedMemory object and repeatedly attempts to attach
 * to the existing System V shared memory segment. The function waits
 * between attempts to avoid busy polling.
 *
 * After a successful attachment, the address of the IPC header located
 * at the beginning of the shared memory segment is stored.
 *
 * @return
 *      - true  Shared memory attached successfully.
 *      - false Failed to obtain the shared memory address.
 */
bool TcpServer::attach_shared_memory()
{
    shared_memory_ = new SharedMemory(shared_memory_key_, IPC_SHARED_MEMORY_SIZE);

    LOG_INFO("Waiting for shared memory.");

    auto start_time = std::chrono::steady_clock::now();

    while (!shared_memory_->attach())
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

        if (elapsed.count() >= IPC_ATTACH_TIMEOUT_MS)
        {
            LOG_ERROR("Timed out while waiting for shared memory.");

            delete shared_memory_;
            shared_memory_ = nullptr;

            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(IPC_RETRY_DELAY_MS));
    }

    ipc_header_ = static_cast<ipc_header_t *>(shared_memory_->data());

    if (ipc_header_ == nullptr)
    {
        LOG_ERROR("Failed to get shared memory address.");

        delete shared_memory_;
        shared_memory_ = nullptr;

        return false;
    }

    LOG_INFO("Shared memory attached.");

    return true;
}

/**
 * @brief Wait until both shared queues are initialized by Database.
 *
 * Repeatedly checks the queue readiness flags stored in the shared
 * memory header. The function returns only after both queues have
 * been completely initialized by Database.
 *
 * A delay is used between checks to avoid busy polling.
 *
 * @return
 *      - true  Both queues are ready.
 *      - false The IPC header is not available.
 */
bool TcpServer::wait_for_queues()
{
    if (ipc_header_ == nullptr)
    {
        LOG_ERROR("IPC header is not available.");

        return false;
    }

    LOG_INFO("Waiting for shared queues.");

    while ((ipc_header_->tcp_to_database_queue_ready != ipc_ready_state_t::READY) ||
           (ipc_header_->database_to_tcp_queue_ready != ipc_ready_state_t::READY))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(IPC_RETRY_DELAY_MS));
    }

    LOG_INFO("Shared queues are ready.");

    return true;
}

/**
 * @brief Attach to the shared queues initialized by Database.
 *
 * Creates local SharedQueue objects for both communication directions.
 * The queues are opened with create set to false because their headers
 * and synchronization resources have already been initialized by
 * Database.
 *
 * @return
 *      - true  Both queues attached successfully.
 *      - false Failed to attach to one of the queues.
 */
bool TcpServer::attach_queues()
{
    if ((shared_memory_ == nullptr) ||
        (!shared_memory_->is_attached()) ||
        (ipc_header_ == nullptr))
    {
        LOG_ERROR("Shared memory is not ready for queue attachment.");

        return false;
    }

    tcp_to_database_queue_ = new SharedQueue(shared_memory_,
                                    TCP_TO_DATABASE_QUEUE_OFFSET,
                                    TCP_TO_DATABASE_SEMAPHORE_NAME,
                                    sizeof(tcp_to_database_message_t),
                                    TCP_TO_DATABASE_QUEUE_CAPACITY,
                                    false);

    if (!tcp_to_database_queue_->is_valid())
    {
        LOG_ERROR("Failed to attach to TcpServer-to-Database queue.");

        delete tcp_to_database_queue_;
        tcp_to_database_queue_ = nullptr;

        return false;
    }

    LOG_INFO("Attached to TcpServer-to-Database queue.");

    database_to_tcp_queue_ = new SharedQueue(shared_memory_,
                                    DATABASE_TO_TCP_QUEUE_OFFSET,
                                    DATABASE_TO_TCP_SEMAPHORE_NAME,
                                    sizeof(database_to_tcp_message_t),
                                    DATABASE_TO_TCP_QUEUE_CAPACITY,
                                    false);

    if (!database_to_tcp_queue_->is_valid())
    {
        LOG_ERROR("Failed to attach to Database-to-TcpServer queue.");

        delete database_to_tcp_queue_;
        database_to_tcp_queue_ = nullptr;

        delete tcp_to_database_queue_;
        tcp_to_database_queue_ = nullptr;

        return false;
    }

    LOG_INFO("Attached to Database-to-TcpServer queue.");

    return true;
}

/**
 * @brief Shut down the TcpServer application.
 *
 * Releases the local shared queue objects, detaches from the shared
 * memory segment and releases local IPC resources.
 *
 * The shared memory segment is not removed because it is owned and
 * managed by the Database application.
 *
 * @return
 *      - true  Shutdown completed successfully.
 *      - false Failed to detach from shared memory.
 */
bool TcpServer::shutdown()
{
    delete database_to_tcp_queue_;
    database_to_tcp_queue_ = nullptr;

    delete tcp_to_database_queue_;
    tcp_to_database_queue_ = nullptr;

    ipc_header_ = nullptr;

    if (shared_memory_ != nullptr)
    {
        if (shared_memory_->is_attached())
        {
            if (!shared_memory_->detach())
            {
                LOG_ERROR("Failed to detach TcpServer from shared memory.");

                return false;
            }
        }

        delete shared_memory_;
        shared_memory_ = nullptr;
    }

    LOG_INFO("TcpServer shutdown completed.");

    logger_close();

    return true;
}
