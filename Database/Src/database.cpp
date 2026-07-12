#include <chrono>
#include <thread>
#include <csignal>
#include <cstdio>
#include "database.hpp"
#include "logger.h"
#include "config.h"


#define DEFAULT_DATABASE_FILE "../Database/parking.db"


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
      enable_console_logging_(DEFAULT_ENABLE_CONSOLE_LOGGING),
      running_(false),
      request_queue_mutex_initialized_(false),
      request_queue_condition_initialized_(false),
      sqlite_database_{},
      sqlite_database_opened_(false)
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

    LOG_INFO(
    "Opening SQLite database '%s'.",
    database_file_.c_str());

    if (sqlite_database_open(
            &sqlite_database_,
            database_file_.c_str()) != 0)
    {
        LOG_ERROR("Failed to open SQLite database.");
        shutdown();
        return false;
    }

    sqlite_database_opened_ = true;

    if (sqlite_database_initialize(&sqlite_database_) != 0)
    {
        LOG_ERROR("Failed to initialize SQLite database schema.");
        shutdown();
        return false;
    }

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
 * Releases local synchronization resources, destroys the shared queue
 * objects, waits until all other processes detach from shared memory,
 * detaches the Database process and removes the shared memory segment.
 *
 * Worker threads must be stopped and joined before this function is called.
 *
 * @return
 *      - true  Shutdown completed successfully.
 *      - false An error occurred during shutdown.
 */
bool Database::shutdown()
{
    int attached_processes;
    int result;

    LOG_INFO("Database shutdown started.");

    /*
     * Mark both shared queues as unavailable before releasing
     * their local objects.
     */
    if (ipc_header_ != nullptr)
    {
        ipc_header_->database_to_tcp_queue_ready =
            ipc_ready_state_t::NOT_READY;

        ipc_header_->tcp_to_database_queue_ready =
            ipc_ready_state_t::NOT_READY;
    }

    ipc_header_ = nullptr;

    /*
     * Release the local SharedQueue objects.
     */
    LOG_INFO("Releasing shared queues.");

    delete database_to_tcp_queue_;
    database_to_tcp_queue_ = nullptr;

    delete tcp_to_database_queue_;
    tcp_to_database_queue_ = nullptr;

    /*
     * Destroy the condition variable used by the internal request queue.
     *
     * The worker threads have already been joined in run(), so no thread
     * can still be waiting on this condition variable.
     */
    if (request_queue_condition_initialized_)
    {
        result = pthread_cond_destroy(&request_queue_condition_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to destroy request queue condition variable. "
                "Error: %d",
                result);

            return false;
        }

        request_queue_condition_initialized_ = false;

        LOG_INFO("Request queue condition variable destroyed.");
    }

    /*
     * Destroy the mutex protecting the internal request queue.
     *
     * The condition variable must be destroyed before its associated
     * mutex.
     */
    if (request_queue_mutex_initialized_)
    {
        result = pthread_mutex_destroy(&request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to destroy request queue mutex. Error: %d",
                result);

            return false;
        }

        request_queue_mutex_initialized_ = false;

        LOG_INFO("Request queue mutex destroyed.");
    }

    /*
     * If shared memory was not initialized, all available local
     * resources have already been released.
     */
    if (shared_memory_ == nullptr)
    {
        LOG_INFO("Database shutdown completed.");

        logger_close();

        return true;
    }

    /*
     * Wait until every other process detaches from the shared memory
     * segment. Database is the owner and must remove it only after all
     * clients have disconnected.
     */
    attached_processes = shared_memory_->attached_count();
    
    if (attached_processes > 1)
	{
		LOG_INFO("Waiting for other processes to detach from shared memory.");
	}

    while (attached_processes > 1)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));

        attached_processes = shared_memory_->attached_count();
    }

    if (attached_processes == -1)
    {
        LOG_ERROR(
            "Failed to get shared memory attachment count.");

        return false;
    }

    /*
     * Detach the Database process from shared memory.
     */
    LOG_INFO("Detaching Database from shared memory.");

    if (shared_memory_->is_attached())
    {
        if (!shared_memory_->detach())
        {
            LOG_ERROR(
                "Failed to detach Database from shared memory.");

            return false;
        }
    }

    /*
     * Remove the System V shared memory segment.
     *
     * Database owns the segment, so this operation is performed only
     * by the Database process.
     */
    LOG_INFO("Removing shared memory segment.");

    if (!shared_memory_->remove())
    {
        LOG_ERROR(
            "Failed to remove shared memory segment.");

        return false;
    }

    delete shared_memory_;
    shared_memory_ = nullptr;

    if (sqlite_database_opened_)
    {
        LOG_INFO("Closing SQLite database.");

        sqlite_database_close(&sqlite_database_);

        sqlite_database_opened_ = false;
    }

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

    database_file_ = config_get_string(&config, "DATABASE_FILE", DEFAULT_DATABASE_FILE);

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

/**
 * @brief Run the Database worker threads.
 *
 * Initializes the synchronization objects, blocks SIGINT in the process
 * threads and starts the request reader and response writer threads.
 *
 * The calling thread waits for SIGINT. After SIGINT is received, the
 * running flag is cleared, waiting worker threads are notified and both
 * worker threads are joined.
 */
void Database::run()
{
    sigset_t signal_set;
    sigset_t previous_signal_set;
    int signal_number;
    int result;


    /**
    *---------------------------------------------------------------
    * Initialize synchronization objects.
    *---------------------------------------------------------------
    */
    result = pthread_mutex_init(&request_queue_mutex_, nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to initialize request queue mutex. Error: %d",
            result);

        return;
    }

    request_queue_mutex_initialized_ = true;

    result = pthread_cond_init(&request_queue_condition_, nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to initialize request queue condition variable. "
            "Error: %d",
            result);

        pthread_mutex_destroy(&request_queue_mutex_);

        return;
    }

    request_queue_condition_initialized_ = true;

    /**
    *---------------------------------------------------------------
    * Block SIGINT for all subsequently created threads.
    *---------------------------------------------------------------
    */
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGUSR1);

    result = pthread_sigmask(
        SIG_BLOCK,
        &signal_set,
        &previous_signal_set);

    if (result != 0)
    {
        LOG_ERROR("Failed to block SIGINT. Error: %d", result);

        pthread_cond_destroy(&request_queue_condition_);

        pthread_mutex_destroy(&request_queue_mutex_);

        return;
    }

    /**
    *---------------------------------------------------------------
    * Start the worker threads.
    *---------------------------------------------------------------
    */
    running_ = true;

    result = pthread_create(
        &request_reader_thread_,
        nullptr,
        Database::request_reader_thread,
        this);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to create request reader thread. Error: %d",
            result);

        running_ = false;

        pthread_sigmask(
            SIG_SETMASK,
            &previous_signal_set,
            nullptr);

        pthread_cond_destroy(&request_queue_condition_);

        pthread_mutex_destroy(&request_queue_mutex_);

        return;
    }

    result = pthread_create(
        &response_writer_thread_,
        nullptr,
        Database::response_writer_thread,
        this);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to create response writer thread. Error: %d",
            result);

        pthread_mutex_lock(&request_queue_mutex_);

        running_ = false;

        pthread_cond_broadcast(&request_queue_condition_);

        pthread_mutex_unlock(&request_queue_mutex_);

        pthread_join(request_reader_thread_, nullptr);

        pthread_sigmask(
            SIG_SETMASK,
            &previous_signal_set,
            nullptr);

        pthread_cond_destroy(&request_queue_condition_);

        pthread_mutex_destroy(&request_queue_mutex_);

        return;
    }


    /**
    *---------------------------------------------------------------
    * Wait until the user requests application termination.
    *---------------------------------------------------------------
    */

    LOG_INFO("Database is running. Press Ctrl+C to stop.");

    while (running_)
    {
        result = sigwait(&signal_set, &signal_number);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed while waiting for a signal. Error: %d",
                result);

            break;
        }

        switch (signal_number)
        {
            case SIGINT:
            {
                LOG_INFO("SIGINT received.");

                running_ = false;

                break;
            }

            case SIGUSR1:
            {
                LOG_INFO("Parking database has been updated.");
                break;
            }

            default:
            {
                LOG_ERROR(
                    "Unexpected signal received: %d.",
                    signal_number);

                break;
            }
        }
    }

    if (result != 0)
    {
        LOG_ERROR("Failed while waiting for a signal. Error: %d", result);
    }

    /**
    *---------------------------------------------------------------
    * Notify all worker threads to terminate.
    *---------------------------------------------------------------
    */

    result = pthread_mutex_lock(&request_queue_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to lock request queue mutex during stop. Error: %d",
            result);
    }
    else
    {
        running_ = false;

        result = pthread_cond_broadcast(&request_queue_condition_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to wake response writer thread. Error: %d",
                result);
        }

        result = pthread_mutex_unlock(&request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to unlock request queue mutex during stop. "
                "Error: %d",
                result);
        }
    }

    /**
    *---------------------------------------------------------------
    * Wait for all worker threads to exit.
    *---------------------------------------------------------------
    */

    LOG_INFO("Joining Database worker threads.");

    result = pthread_join(request_reader_thread_, nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to join request reader thread. Error: %d",
            result);
    }

    result = pthread_join(response_writer_thread_, nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to join response writer thread. Error: %d",
            result);
    }

    /**
    *---------------------------------------------------------------
    * Restore the previous signal mask.
    *---------------------------------------------------------------
    */

    pthread_sigmask(
        SIG_SETMASK,
        &previous_signal_set,
        nullptr);

    LOG_INFO("Database worker threads stopped.");
}

/**
 * @brief Read requests from the shared request queue.
 *
 * Continuously receives requests from the TcpServer-to-Database
 * shared queue and places them into the internal Database queue.
 */
void Database::request_reader_loop()
{
    tcp_to_database_message_t request{};
    int result;

    LOG_INFO("Request reader thread started.");

    while (running_)
    {
        if (!tcp_to_database_queue_->pop(&request))
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(IPC_RETRY_DELAY_MS));

            continue;
        }

        result = pthread_mutex_lock(&request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to lock internal request queue mutex. Error: %d",
                result);

            break;
        }

        request_queue_.push(request);

        result = pthread_mutex_unlock(&request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to unlock internal request queue mutex. Error: %d",
                result);

            break;
        }

        result = pthread_cond_signal(&request_queue_condition_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to signal response writer thread. Error: %d",
                result);

            break;
        }

        LOG_INFO(
            "Request %llu added to the internal Database queue.",
            static_cast<unsigned long long>(request.request_id));
    }

    LOG_INFO("Request reader thread stopped.");
}

/**
 * @brief Entry point for the request reader thread.
 *
 * Converts the generic thread argument back to a Database object
 * and starts the request reader loop.
 *
 * @param argument Pointer to the Database object.
 *
 * @return Always returns nullptr.
 */
void* Database::request_reader_thread(void* argument)
{
    Database* database = static_cast<Database*>(argument);

    database->request_reader_loop();

    return nullptr;
}

/**
 * @brief Read pending requests and send test responses to TcpServer.
 *
 * Waits until a request is available in the internal Database queue,
 * removes the request and sends a simple test response through the
 * Database-to-TcpServer shared queue.
 *
 * No request processing or Database access is performed at this stage.
 */
void Database::response_writer_loop()
{
    tcp_to_database_message_t request{};
    database_to_tcp_message_t response{};
    int result;

    LOG_INFO("Response writer thread started.");

    while (running_)
    {
        result = pthread_mutex_lock(&request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to lock internal request queue mutex. Error: %d", result);

            break;
        }

        result = 0;
        while (request_queue_.empty() && running_)
        {
            result = pthread_cond_wait(&request_queue_condition_, &request_queue_mutex_);

            if (result != 0)
            {
                LOG_ERROR(
                    "Failed to wait for an internal request. Error: %d", result);

                break;
            }
        }

        if ((result != 0) ||
            (!running_ && request_queue_.empty()))
        {
            pthread_mutex_unlock(&request_queue_mutex_);
            break;
        }

        request = request_queue_.front();
        request_queue_.pop();

        result = pthread_mutex_unlock(&request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to unlock internal request queue mutex. Error: %d", result);

            break;
        }

        response = {};

        process_database_request(request, response);

        if (!database_to_tcp_queue_->push(&response))
        {
            LOG_ERROR("Failed to send response %llu to TcpServer.",
                static_cast<unsigned long long>(response.request_id));

            continue;
        }

        LOG_INFO("Response %llu sent to TcpServer.",
            static_cast<unsigned long long>(response.request_id));
    }

    LOG_INFO("Response writer thread stopped.");
}

/**
 * @brief Entry point for the response writer thread.
 *
 * Converts the generic thread argument back to a Database object
 * and starts the response writer loop.
 *
 * @param argument Pointer to the Database object.
 *
 * @return Always returns nullptr.
 */
void* Database::response_writer_thread(void* argument)
{
    Database* database = static_cast<Database*>(argument);

    database->response_writer_loop();

    return nullptr;
}

void Database::process_database_request(const tcp_to_database_message_t& request, database_to_tcp_message_t& response)
{
    response = {};

    response.request_id = request.request_id;

    std::snprintf(
        response.vehicle_number,
        sizeof(response.vehicle_number),
        "%s",
        request.vehicle_number);

    switch (request.action)
    {
        case parking_action_t::START_PARKING:
        {
            process_start_parking_request(request, response);

            break;
        }

        case parking_action_t::END_PARKING:
        {
            process_end_parking_request(request, response);

            break;
        }

        case parking_action_t::ACKNOWLEDGE:
        default:
        {
            LOG_ERROR(
                "Invalid parking action received in request %llu.",
                static_cast<unsigned long long>(request.request_id));

            response.action = request.action;
            response.status = ipc_status_t::INVALID_REQUEST;

            break;
        }
    }
}

/**
 * @brief Processes a parking start request.
 *
 * Finds the parking city using the received coordinates and creates
 * a new active parking session in the SQLite database.
 *
 * If the parking session is created successfully, an ACKNOWLEDGE
 * response is prepared for TcpServer.
 *
 * @param request Parking start request received from TcpServer.
 * @param response Response to be sent back to TcpServer.
 */
void Database::process_start_parking_request(
    const tcp_to_database_message_t& request,
    database_to_tcp_message_t& response)
{
    int city_id;
    int result;

    /*
     * Find the city whose coordinate area contains
     * the received vehicle coordinates.
     */
    result = sqlite_database_find_city(
        &sqlite_database_,
        request.latitude,
        request.longitude,
        &city_id);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to find a parking city for request %llu. "
            "Coordinates: %.2f, %.2f.",
            static_cast<unsigned long long>(request.request_id),
            request.latitude,
            request.longitude);

        response.action = parking_action_t::ACKNOWLEDGE;
        response.status = ipc_status_t::INVALID_REQUEST;

        return;
    }

    /*
     * Create a new active parking session.
     *
     * SQLiteDatabase retrieves and stores the parking price
     * that is active at the moment the session starts.
     */
    result = sqlite_database_start_parking(
        &sqlite_database_,
        request.vehicle_number,
        request.latitude,
        request.longitude,
        city_id,
        request.parking_start_time);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to start parking for vehicle '%s'. "
            "Request ID: %llu.",
            request.vehicle_number,
            static_cast<unsigned long long>(request.request_id));

        response.action = parking_action_t::ACKNOWLEDGE;
        response.status = ipc_status_t::DATABASE_ERROR;

        return;
    }

    /*
     * Prepare the successful parking registration response.
     */
    response.action = parking_action_t::ACKNOWLEDGE;
    response.status = ipc_status_t::SUCCESS;
    response.parking_start_time = request.parking_start_time;

    LOG_INFO(
        "Parking start request %llu processed successfully "
        "for vehicle '%s'. City ID: %d.",
        static_cast<unsigned long long>(request.request_id),
        request.vehicle_number,
        city_id);
}

/**
 * @brief Processes a parking end request.
 *
 * Completes the active parking session for the specified vehicle,
 * retrieves the original parking start time from SQLite, calculates
 * the parking duration, and prepares the response for TcpServer.
 *
 * @param request Parking end request received from TcpServer.
 * @param response Response to be sent back to TcpServer.
 */
void Database::process_end_parking_request(
    const tcp_to_database_message_t& request,
    database_to_tcp_message_t& response)
{
    std::int64_t parking_start_time;
    std::int64_t parking_duration;
    double parking_cost;
    int result;

    /*
     * Complete the active parking session and retrieve
     * its stored start time and calculated cost.
     */
    result = sqlite_database_stop_parking(
        &sqlite_database_,
        request.vehicle_number,
        request.parking_end_time,
        &parking_start_time,
        &parking_cost);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to stop parking for vehicle '%s'. "
            "Request ID: %llu.",
            request.vehicle_number,
            static_cast<unsigned long long>(
                request.request_id));

        response.action = parking_action_t::END_PARKING;

        response.status = ipc_status_t::VEHICLE_NOT_FOUND;

        return;
    }

    /*
     * Calculate the parking duration using the start time
     * stored in the database.
     */
    parking_duration = request.parking_end_time - parking_start_time;

    /*
     * Prepare the successful parking completion response.
     */
    response.action = parking_action_t::END_PARKING;

    response.status = ipc_status_t::SUCCESS;

    response.parking_start_time = parking_start_time;

    response.parking_end_time = request.parking_end_time;

    response.parking_duration = parking_duration;

    response.parking_cost = parking_cost;

    LOG_INFO(
        "Parking end request %llu processed successfully "
        "for vehicle '%s'. Duration: %lld second(s), "
        "cost: %.2f.",
        static_cast<unsigned long long>(
            request.request_id),
        request.vehicle_number,
        static_cast<long long>(
            parking_duration),
        parking_cost);
}
