#include "tcp_server.hpp"
#include <chrono>
#include <cstring>
#include <thread>
#include <csignal>

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
 * memory segment and releases all local IPC resources.
 *
 * Worker threads must be stopped and joined before this function
 * is called.
 *
 * The shared memory segment is not removed because it is owned
 * and managed by the Database application.
 *
 * @return
 *      - true  Shutdown completed successfully.
 *      - false Failed to detach from shared memory.
 */
bool TcpServer::shutdown()
{
    LOG_INFO("TcpServer shutdown started.");

    /*
     * Release the local SharedQueue objects.
     *
     * The worker threads have already been joined in run(), so they
     * can no longer access these queues.
     */
    LOG_INFO("Releasing shared queues.");

    delete database_to_tcp_queue_;
    database_to_tcp_queue_ = nullptr;

    delete tcp_to_database_queue_;
    tcp_to_database_queue_ = nullptr;

    /*
     * The IPC header is located inside shared memory.
     * Clear the local pointer before detaching from the segment.
     */
    ipc_header_ = nullptr;

    /*
     * Detach TcpServer from the shared memory segment.
     *
     * TcpServer is not the owner of the segment and therefore must
     * not remove it.
     */
    if (shared_memory_ != nullptr)
    {
        LOG_INFO("Detaching TcpServer from shared memory.");

        if (shared_memory_->is_attached())
        {
            if (!shared_memory_->detach())
            {
                LOG_ERROR(
                    "Failed to detach TcpServer from shared memory.");

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


/**
 * @brief Run the TcpServer worker threads.
 *
 * Blocks SIGINT, creates the request writer and response reader
 * threads and waits until the user requests application termination.
 *
 * After SIGINT is received, the function clears the running flag,
 * waits for both worker threads to terminate and restores the
 * previous signal mask.
 */
void TcpServer::run()
{
    sigset_t signal_set;
    sigset_t previous_signal_set;
    int signal_number = 0;
    int result;

    /*
     * Prepare a signal set containing SIGINT.
     */
    result = sigemptyset(&signal_set);

    if (result != 0)
    {
        LOG_ERROR("Failed to initialize signal set.");

        return;
    }

    result = sigaddset(&signal_set, SIGINT);

    if (result != 0)
    {
        LOG_ERROR("Failed to add SIGINT to signal set.");

        return;
    }

    /*
     * Block SIGINT before creating worker threads.
     *
     * The created threads inherit this signal mask, allowing the
     * main thread to receive SIGINT synchronously through sigwait().
     */
    result = pthread_sigmask(
        SIG_BLOCK,
        &signal_set,
        &previous_signal_set);

    if (result != 0)
    {
        LOG_ERROR("Failed to block SIGINT. Error: %d", result);

        return;
    }

    /*
     * Enable execution of the worker thread loops.
     */
    running_ = true;

    /*
     * Create the thread responsible for generating and sending
     * requests to Database.
     */
    result = pthread_create(
        &request_writer_thread_,
        nullptr,
        TcpServer::request_writer_thread,
        this);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to create request writer thread. Error: %d",
            result);

        running_ = false;

        pthread_sigmask(
            SIG_SETMASK,
            &previous_signal_set,
            nullptr);

        return;
    }

    LOG_INFO("Request writer thread created.");

    /*
     * Create the thread responsible for receiving responses
     * from Database.
     */
    result = pthread_create(
        &response_reader_thread_,
        nullptr,
        TcpServer::response_reader_thread,
        this);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to create response reader thread. Error: %d",
            result);

        running_ = false;

        /*
         * The request writer thread was already created, so wait
         * until it observes running_ == false and terminates.
         */
        result = pthread_join(request_writer_thread_, nullptr);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to join request writer thread. Error: %d",
                result);
        }

        pthread_sigmask(
            SIG_SETMASK,
            &previous_signal_set,
            nullptr);

        return;
    }

    LOG_INFO("Response reader thread created.");

    /*
     * Wait for the user to request application termination.
     *
     * sigwait() blocks only the calling thread. Both worker threads
     * continue sending requests and receiving responses.
     */
    LOG_INFO("TcpServer is running. Press Ctrl+C to stop.");

    result = sigwait(&signal_set, &signal_number);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed while waiting for SIGINT. Error: %d",
            result);
    }
    else
    {
        LOG_INFO("SIGINT received.");
    }

    /*
     * Request both worker threads to terminate their main loops.
     */
    running_ = false;

    /*
     * Wait until both worker threads have stopped before run()
     * returns and shutdown() starts releasing IPC resources.
     */
    LOG_INFO("Joining TcpServer worker threads.");

    result = pthread_join(request_writer_thread_, nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to join request writer thread. Error: %d",
            result);
    }

    result = pthread_join(response_reader_thread_, nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to join response reader thread. Error: %d",
            result);
    }

    /*
     * Restore the signal mask that was active before run().
     */
    result = pthread_sigmask(
        SIG_SETMASK,
        &previous_signal_set,
        nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to restore previous signal mask. Error: %d",
            result);
    }

    LOG_INFO("TcpServer worker threads stopped.");
}

/**
 * @brief Generate test requests and send them to Database.
 *
 * Continuously creates test parking requests and sends them through
 * the TcpServer-to-Database shared queue.
 *
 * Requests are generated with increasing request identifiers.
 * A delay is used between transmissions to avoid filling the queue
 * too quickly during testing.
 */
void TcpServer::request_writer_loop()
{
    std::uint64_t request_id = 1U;

    LOG_INFO("Request writer thread started.");

    while (running_)
    {
        tcp_to_database_message_t request{};

        request.request_id = request_id;
        request.action = parking_action_t::START_PARKING;

        std::strncpy(
            request.vehicle_number,
            "123-45-678",
            VEHICLE_NUMBER_SIZE - 1U);

        request.vehicle_number[VEHICLE_NUMBER_SIZE - 1U] = '\0';

        std::strncpy(
            request.city,
            "Haifa",
            CITY_NAME_SIZE - 1U);

        request.city[CITY_NAME_SIZE - 1U] = '\0';

        request.latitude = 32.7940;
        request.longitude = 34.9896;
        request.parking_start_time = 1000;
        request.parking_end_time = 0;
        request.parking_cost = 0.0;

        if (!tcp_to_database_queue_->push(&request))
        {
            LOG_ERROR(
                "Failed to send request %llu to Database.",
                static_cast<unsigned long long>(request.request_id));

            std::this_thread::sleep_for(
                std::chrono::milliseconds(IPC_RETRY_DELAY_MS));

            continue;
        }

        LOG_INFO(
            "Request %llu sent to Database.",
            static_cast<unsigned long long>(request.request_id));

        ++request_id;

        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    LOG_INFO("Request writer thread stopped.");
}

/**
 * @brief Entry point for the request writer thread.
 *
 * Converts the generic thread argument back to a TcpServer object
 * and starts the request writer loop.
 *
 * @param argument Pointer to the TcpServer object.
 *
 * @return Always returns nullptr.
 */
void* TcpServer::request_writer_thread(void* argument)
{
    TcpServer* tcp_server = static_cast<TcpServer*>(argument);

    tcp_server->request_writer_loop();

    return nullptr;
}

/**
 * @brief Read responses received from Database.
 *
 * Continuously attempts to read responses from the
 * Database-to-TcpServer shared queue.
 *
 * Successfully received responses are validated and logged.
 * No response routing to TCP clients is performed at this stage.
 */
void TcpServer::response_reader_loop()
{
    database_to_tcp_message_t response{};

    LOG_INFO("Response reader thread started.");

    while (running_)
    {
        if (!database_to_tcp_queue_->pop(&response))
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(IPC_RETRY_DELAY_MS));

            continue;
        }

        if (response.status != ipc_status_t::SUCCESS)
        {
            LOG_ERROR(
                "Database failed to process request %llu.",
                static_cast<unsigned long long>(response.request_id));

            continue;
        }

        LOG_INFO(
            "Response %llu received successfully. "
            "Parking duration: %u, parking cost: %.2f",
            static_cast<unsigned long long>(response.request_id),
            response.parking_duration,
            response.parking_cost);
    }

    LOG_INFO("Response reader thread stopped.");
}

/**
 * @brief Entry point for the response reader thread.
 *
 * Converts the generic thread argument back to a TcpServer object
 * and starts the response reader loop.
 *
 * @param argument Pointer to the TcpServer object.
 *
 * @return Always returns nullptr.
 */
void* TcpServer::response_reader_thread(void* argument)
{
    TcpServer* tcp_server = static_cast<TcpServer*>(argument);

    tcp_server->response_reader_loop();

    return nullptr;
}

