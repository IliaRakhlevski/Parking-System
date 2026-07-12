#include "tcp_server.hpp"
#include <chrono>
#include <cstring>
#include <thread>
#include <csignal>
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>



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
      enable_console_logging_(DEFAULT_ENABLE_CONSOLE_LOGGING),
      request_queue_mutex_initialized_(false),
      request_queue_condition_initialized_(false),
      clients_mutex_initialized_(false),
      running_(false),
      server_socket_(-1),
      server_port_(DEFAULT_TCP_SERVER_PORT),
      listen_backlog_(DEFAULT_TCP_LISTEN_BACKLOG)
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
    
    server_port_ = static_cast<std::uint16_t>(config_get_int(
                            &config,
                            "TCP_SERVER_PORT",
                            DEFAULT_TCP_SERVER_PORT));

    listen_backlog_ = config_get_int(
                            &config,
                            "TCP_LISTEN_BACKLOG",
                            DEFAULT_TCP_LISTEN_BACKLOG);

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

    /*
    * Initialize synchronization resources used by TcpServer threads.
    */
    int result = pthread_mutex_init(
        &request_queue_mutex_,
        nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to initialize request queue mutex. Error: %d",
            result);

        return false;
    }

    request_queue_mutex_initialized_ = true;

    result = pthread_cond_init(
        &request_queue_condition_,
        nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to initialize request queue condition variable. "
            "Error: %d",
            result);

        pthread_mutex_destroy(&request_queue_mutex_);
        request_queue_mutex_initialized_ = false;

        return false;
    }

    request_queue_condition_initialized_ = true;

    result = pthread_mutex_init(
        &clients_mutex_,
        nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to initialize clients mutex. Error: %d",
            result);

        pthread_cond_destroy(&request_queue_condition_);
        request_queue_condition_initialized_ = false;

        pthread_mutex_destroy(&request_queue_mutex_);
        request_queue_mutex_initialized_ = false;

        return false;
    }

    clients_mutex_initialized_ = true;

    LOG_INFO("TcpServer synchronization initialized successfully.");

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
    int result;
    bool shutdown_successful = true;

    LOG_INFO("TcpServer shutdown started.");

    /*
     * Destroy the request queue condition variable.
     *
     * All worker threads have already been joined in run(), so the
     * condition variable is no longer in use.
     */
    if (request_queue_condition_initialized_)
    {
        result = pthread_cond_destroy(
            &request_queue_condition_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to destroy request queue condition variable. "
                "Error: %d",
                result);

            shutdown_successful = false;
        }
        else
        {
            request_queue_condition_initialized_ = false;

            LOG_INFO(
                "Request queue condition variable destroyed.");
        }
    }

    /*
     * Destroy the request queue mutex.
     */
    if (request_queue_mutex_initialized_)
    {
        result = pthread_mutex_destroy(
            &request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to destroy request queue mutex. Error: %d",
                result);

            shutdown_successful = false;
        }
        else
        {
            request_queue_mutex_initialized_ = false;

            LOG_INFO("Request queue mutex destroyed.");
        }
    }

    /*
     * Destroy the clients table mutex.
     */
    if (clients_mutex_initialized_)
    {
        result = pthread_mutex_destroy(
            &clients_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to destroy clients table mutex. Error: %d",
                result);

            shutdown_successful = false;
        }
        else
        {
            clients_mutex_initialized_ = false;

            LOG_INFO("Clients table mutex destroyed.");
        }
    }

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

                shutdown_successful = false;
            }
        }

        delete shared_memory_;
        shared_memory_ = nullptr;
    }

    if (shutdown_successful)
    {
        LOG_INFO("TcpServer shutdown completed.");
    }
    else
    {
        LOG_ERROR("TcpServer shutdown completed with errors.");
    }

    logger_close();

    return shutdown_successful;
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

    result = pthread_create(
        &tcp_server_thread_,
        nullptr,
        TcpServer::tcp_server_thread,
        this);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to create TCP server thread. Error: %d",
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

        /*
         * The response reader thread was already created, so wait
         * until it observes running_ == false and terminates.
         */
        result = pthread_join(response_reader_thread_, nullptr);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to join response reader thread. Error: %d",
                result);
        }

        pthread_sigmask(
            SIG_SETMASK,
            &previous_signal_set,
            nullptr);

        return;
    }

    LOG_INFO("TCP server thread created.");

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

    result = pthread_mutex_lock(&request_queue_mutex_);

    if (result == 0)
    {
        pthread_cond_broadcast(&request_queue_condition_);
        pthread_mutex_unlock(&request_queue_mutex_);
    }
    else
    {
        LOG_ERROR(
            "Failed to lock request queue mutex during shutdown. "
            "Error: %d",
            result);
    }

    /*
     * Wait until both worker threads have stopped before run()
     * returns and shutdown() starts releasing IPC resources.
     */
    LOG_INFO("Joining TcpServer worker threads.");

    if (server_socket_ != -1)
    {
        ::shutdown(server_socket_, SHUT_RDWR);
    }

    result = pthread_join(tcp_server_thread_, nullptr);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to join tcp server thread. Error: %d",
            result);
    }

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
 * @brief Receive a parking request from a connected TCP client.
 *
 * Reads one complete parking request from the specified client socket
 * and places it into the internal request queue.
 *
 * TCP is a stream protocol, so the request may be received in several
 * parts. The function continues calling recv() until the entire request
 * has been received.
 *
 * After the request is queued, the request writer thread is notified
 * through the request queue condition variable.
 *
 * @param client_socket Client socket file descriptor.
 *
 * @return
 *      - true  Request received and queued successfully.
 *      - false Client disconnected or a fatal error occurred.
 */
bool TcpServer::receive_client_request(int client_socket)
{
    tcp_to_database_message_t request{};
    std::size_t total_bytes_received = 0U;
    int result;

    /*
     * Receive the complete request.
     *
     * A single recv() call is not guaranteed to return the entire
     * structure because TCP provides a continuous byte stream.
     */
    while (total_bytes_received < sizeof(request))
    {
        const ssize_t bytes_received = recv(
            client_socket,
            reinterpret_cast<char*>(&request) +
                total_bytes_received,
            sizeof(request) - total_bytes_received,
            0);

        if (bytes_received == 0)
        {
            LOG_INFO(
                "TCP client socket %d disconnected.",
                client_socket);

            return false;
        }

        if (bytes_received == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LOG_ERROR(
                "Failed to receive request from TCP client socket %d. "
                "Error: %s",
                client_socket,
                std::strerror(errno));

            return false;
        }

        total_bytes_received +=
            static_cast<std::size_t>(bytes_received);
    }

    LOG_INFO(
        "Request %llu received from TCP client socket %d.",
        static_cast<unsigned long long>(request.request_id),
        client_socket);

    /*
    * Associate the request identifier with the client socket.
    *
    * This mapping is used later by the response reader thread to
    * deliver the Database response to the correct TCP client.
    */
    result = pthread_mutex_lock(&clients_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to lock clients mutex. Error: %d",
            result);

        return false;
    }

    pending_requests_[request.request_id] = client_socket;

    result = pthread_mutex_unlock(&clients_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to unlock clients mutex. Error: %d",
            result);

        return false;
    }

    /*
     * Lock the internal request queue before inserting the new
     * client request.
     */
    result = pthread_mutex_lock(&request_queue_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to lock request queue mutex. Error: %d",
            result);

        return false;
    }

    request_queue_.push(request);

    /*
     * Notify the request writer thread that a new request is
     * available.
     */
    result = pthread_cond_signal(&request_queue_condition_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to signal request queue condition. Error: %d",
            result);

        pthread_mutex_unlock(&request_queue_mutex_);

        return false;
    }

    result = pthread_mutex_unlock(&request_queue_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to unlock request queue mutex. Error: %d",
            result);

        return false;
    }

    LOG_INFO(
        "Request %llu added to the internal request queue.",
        static_cast<unsigned long long>(request.request_id));

    return true;
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
 * @brief Read Database responses and send them to TCP clients.
 *
 * Receives responses from the Database-to-TcpServer shared queue,
 * finds the client associated with each request identifier and sends
 * the response to that client through its TCP socket.
 */
void TcpServer::response_reader_loop()
{
    database_to_tcp_message_t response{};

    LOG_INFO("Response reader thread started.");

    while (running_)
    {
        if (!database_to_tcp_queue_->pop(&response))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(IPC_RETRY_DELAY_MS));

            continue;
        }

        int result;
        int client_socket = -1;

        /*
         * Find the client associated with this request.
         */
        result = pthread_mutex_lock(&clients_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to lock clients mutex. Error: %d",
                result);

            continue;
        }

        auto pending_request = pending_requests_.find(response.request_id);

        if (pending_request == pending_requests_.end())
        {
            pthread_mutex_unlock(&clients_mutex_);

            LOG_ERROR(
                "No TCP client found for response %llu.",
                static_cast<unsigned long long>(
                    response.request_id));

            continue;
        }

        client_socket = pending_request->second;

        /*
         * Remove the completed request from the pending table.
         */
        pending_requests_.erase(pending_request);

        result = pthread_mutex_unlock(&clients_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to unlock clients mutex. Error: %d",
                result);

            continue;
        }

        /*
         * Send the complete response to the client.
         */
        std::size_t total_bytes_sent = 0U;

        while (total_bytes_sent < sizeof(response))
        {
            const ssize_t bytes_sent = send(
                client_socket,
                reinterpret_cast<const char*>(&response) +
                    total_bytes_sent,
                sizeof(response) - total_bytes_sent,
                MSG_NOSIGNAL);

            if (bytes_sent == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                LOG_ERROR(
                    "Failed to send response %llu to TCP client "
                    "socket %d. Error: %s",
                    static_cast<unsigned long long>(
                        response.request_id),
                    client_socket,
                    std::strerror(errno));

                break;
            }

            total_bytes_sent +=
                static_cast<std::size_t>(bytes_sent);
        }

        if (total_bytes_sent != sizeof(response))
        {
            continue;
        }

        if (response.status != ipc_status_t::SUCCESS)
        {
            LOG_ERROR(
                "Database failed to process request %llu.",
                static_cast<unsigned long long>(
                    response.request_id));

            continue;
        }

        LOG_INFO(
            "Response %llu sent successfully to TCP client socket %d. "
            "Parking duration: %u, parking cost: %.2f",
            static_cast<unsigned long long>(
                response.request_id),
            client_socket,
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

/**
 * @brief Accept and process multiple TCP client connections.
 *
 * Creates, configures and binds the TCP server socket. The function
 * uses select() to monitor the listening socket and all connected
 * client sockets from a single worker thread.
 *
 * When the listening socket becomes ready, a new client connection
 * is accepted, registered and acknowledged.
 *
 * When a client socket becomes ready, the client's request is read
 * and prepared for transmission to Database.
 */
void TcpServer::tcp_server_loop()
{
    sockaddr_in server_address{};
    fd_set master_socket_set;
    fd_set ready_socket_set;
    int maximum_socket_descriptor;
    int socket_option = 1;
    int result;

    LOG_INFO("TCP server thread started.");

    /*
     * Create an IPv4 TCP socket.
     */
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket_ == -1)
    {
        LOG_ERROR(
            "Failed to create TCP server socket. Error: %s",
            std::strerror(errno));

        running_ = false;

        return;
    }

    LOG_INFO("TCP server socket created.");

    /*
     * Allow the server to reuse a recently released local address.
     */
    result = setsockopt(
        server_socket_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &socket_option,
        sizeof(socket_option));

    if (result == -1)
    {
        LOG_ERROR(
            "Failed to set SO_REUSEADDR on TCP server socket. "
            "Error: %s",
            std::strerror(errno));

        close(server_socket_);
        server_socket_ = -1;

        running_ = false;

        return;
    }

    /*
     * Configure the server to accept connections through any
     * available IPv4 network interface.
     */
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(server_port_);

    /*
     * Bind the server socket to the configured TCP port.
     */
    result = bind(
        server_socket_,
        reinterpret_cast<sockaddr*>(&server_address),
        sizeof(server_address));

    if (result == -1)
    {
        LOG_ERROR(
            "Failed to bind TCP server socket to port %u. Error: %s",
            static_cast<unsigned int>(server_port_),
            std::strerror(errno));

        close(server_socket_);
        server_socket_ = -1;

        running_ = false;

        return;
    }

    LOG_INFO(
        "TCP server socket bound to port %u.",
        static_cast<unsigned int>(server_port_));

    /*
     * Start listening for incoming TCP client connections.
     */
    result = listen(server_socket_, listen_backlog_);

    if (result == -1)
    {
        LOG_ERROR(
            "Failed to listen on TCP server socket. Error: %s",
            std::strerror(errno));

        close(server_socket_);
        server_socket_ = -1;

        running_ = false;

        return;
    }

    LOG_INFO(
        "TCP server is listening on port %u.",
        static_cast<unsigned int>(server_port_));

    /*
     * Initialize the master descriptor set.
     *
     * The listening socket is always monitored because it becomes
     * readable when a new client connection is waiting.
     */
    FD_ZERO(&master_socket_set);
    FD_SET(server_socket_, &master_socket_set);

    maximum_socket_descriptor = server_socket_;

    while (running_)
    {
        /*
         * select() modifies the descriptor sets passed to it.
         * Therefore, a copy of the master set must be created before
         * every call.
         */
        ready_socket_set = master_socket_set;


        result = select(
            maximum_socket_descriptor + 1,
            &ready_socket_set,
            nullptr,
            nullptr,
            nullptr);

        if (result == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LOG_ERROR(
                "Failed while waiting for TCP socket activity. "
                "Error: %s",
                std::strerror(errno));

            running_ = false;

            break;
        }

        /*
         * Check every monitored descriptor reported by select().
         */
        for (int socket_descriptor = 0;
             socket_descriptor <= maximum_socket_descriptor;
             ++socket_descriptor)
        {
            if (!FD_ISSET(socket_descriptor, &ready_socket_set))
            {
                continue;
            }

            /*
             * The listening socket is ready when a new client is
             * waiting to be accepted.
             */
            if (socket_descriptor == server_socket_)
            {
                sockaddr_in client_address{};
                socklen_t client_address_length =
                    sizeof(client_address);

                int client_socket = accept(
                    server_socket_,
                    reinterpret_cast<sockaddr*>(&client_address),
                    &client_address_length);

                if (client_socket == -1)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }

                    LOG_ERROR(
                        "Failed to accept TCP client connection. "
                        "Error: %s",
                        std::strerror(errno));

                    continue;
                }

                /*
                 * select() cannot monitor descriptors greater than
                 * or equal to FD_SETSIZE.
                 */
                if (client_socket >= FD_SETSIZE)
                {
                    LOG_ERROR(
                        "TCP client socket %d exceeds FD_SETSIZE.",
                        client_socket);

                    close(client_socket);

                    continue;
                }

                char client_address_string[INET_ADDRSTRLEN]{};

                const char* address_result = inet_ntop(
                    AF_INET,
                    &client_address.sin_addr,
                    client_address_string,
                    sizeof(client_address_string));

                if (address_result != nullptr)
                {
                    LOG_INFO(
                        "TCP client connected. Address: %s, "
                        "port: %u, socket: %d.",
                        client_address_string,
                        static_cast<unsigned int>(
                            ntohs(client_address.sin_port)),
                        client_socket);
                }
                else
                {
                    LOG_INFO(
                        "TCP client connected. Socket: %d.",
                        client_socket);
                }

                /*
                 * Store the connected client's information.
                 */
                if (!add_client(client_socket))
                {
                    LOG_ERROR(
                        "Failed to register TCP client socket %d.",
                        client_socket);

                    close(client_socket);

                    continue;
                }

                /*
                 * Add the new client socket to the set monitored
                 * by select().
                 */
                FD_SET(client_socket, &master_socket_set);

                if (client_socket > maximum_socket_descriptor)
                {
                    maximum_socket_descriptor = client_socket;
                }

                LOG_INFO(
                    "TCP client socket %d registered successfully.",
                    client_socket);

                continue;
            }

            /*
             * A connected client socket is ready for reading.
             *
             * receive_client_request() returns false when the client
             * disconnects or when a fatal socket error occurs.
             */
            if (!receive_client_request(socket_descriptor))
            {
                LOG_INFO(
                    "Disconnecting TCP client socket %d.",
                    socket_descriptor);

                FD_CLR(
                    socket_descriptor,
                    &master_socket_set);

                remove_client(socket_descriptor);

                close(socket_descriptor);

                /*
                 * Recalculate the highest monitored descriptor when
                 * the removed client had the highest descriptor.
                 */
                if (socket_descriptor ==
                    maximum_socket_descriptor)
                {
                    while ((maximum_socket_descriptor >= 0) &&
                           (!FD_ISSET(
                               maximum_socket_descriptor,
                               &master_socket_set)))
                    {
                        --maximum_socket_descriptor;
                    }
                }
            }
        }
    }

    /*
     * Close all connected client sockets before the TCP server
     * thread terminates.
     *
     * Client table cleanup will later be placed in a separate
     * helper function.
     */
    for (int socket_descriptor = 0;
         socket_descriptor <= maximum_socket_descriptor;
         ++socket_descriptor)
    {
        if ((socket_descriptor != server_socket_) &&
            FD_ISSET(socket_descriptor, &master_socket_set))
        {
            ::shutdown(socket_descriptor, SHUT_RDWR);
            close(socket_descriptor);
        }
    }

    if (server_socket_ != -1)
    {
        close(server_socket_);
        server_socket_ = -1;
    }

    LOG_INFO("TCP server thread stopped.");
}

/**
 * @brief Entry point for the TCP server thread.
 *
 * @param argument Pointer to the TcpServer object.
 *
 * @return Always returns nullptr.
 */
void* TcpServer::tcp_server_thread(void* argument)
{
    TcpServer* tcp_server = static_cast<TcpServer*>(argument);

    tcp_server->tcp_server_loop();

    return nullptr;
}

/**
 * @brief Add a connected TCP client to the clients table.
 *
 * Stores the client's socket descriptor and IPv4 address information.
 * Access to the clients table is protected by a mutex because the
 * table may be accessed by multiple TcpServer worker threads.
 *
 * @param client_socket Client socket file descriptor.
 * @param client_address Client IPv4 address information.
 *
 * @return
 *      - true  Client added successfully.
 *      - false Client is already registered or mutex operation failed.
 */
bool TcpServer::add_client(int client_socket)
{
    int result;

    if (client_socket < 0)
    {
        LOG_ERROR(
            "Cannot register an invalid TCP client socket.");

        return false;
    }

    result = pthread_mutex_lock(&clients_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to lock clients table mutex. Error: %d",
            result);

        return false;
    }

    /*
     * Ensure that the socket descriptor is not already registered.
     */
    if (clients_.find(client_socket) != clients_.end())
    {
        LOG_ERROR(
            "TCP client socket %d is already registered.",
            client_socket);

        result = pthread_mutex_unlock(&clients_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to unlock clients table mutex. Error: %d",
                result);
        }

        return false;
    }

    tcp_client_t client{};

    client.socket_fd = client_socket;

    clients_.emplace(client_socket, client);

    result = pthread_mutex_unlock(&clients_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to unlock clients table mutex. Error: %d",
            result);

        return false;
    }

    LOG_INFO(
        "TCP client socket %d added to clients table.",
        client_socket);

    return true;
}

/**
 * @brief Remove a TCP client from the clients table.
 *
 * Removes the client information associated with the specified socket.
 * The socket itself is not closed by this function.
 *
 * @param client_socket Client socket file descriptor.
 */
void TcpServer::remove_client(int client_socket)
{
    int result;

    result = pthread_mutex_lock(&clients_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to lock clients table mutex. Error: %d",
            result);

        return;
    }

    const std::size_t removed_clients = clients_.erase(client_socket);

    result = pthread_mutex_unlock(&clients_mutex_);

    if (result != 0)
    {
        LOG_ERROR(
            "Failed to unlock clients table mutex. Error: %d",
            result);
    }

    if (removed_clients == 0U)
    {
        LOG_ERROR(
            "TCP client socket %d was not found in clients table.",
            client_socket);

        return;
    }

    LOG_INFO(
        "TCP client socket %d removed from clients table.",
        client_socket);
}

/**
 * @brief Forward client requests to Database.
 *
 * Waits for client requests to appear in the internal request queue.
 * Each queued request is removed and forwarded to Database through
 * the TcpServer-to-Database shared queue.
 *
 * The internal queue is protected by a mutex. A condition variable
 * prevents the thread from consuming processor time while the queue
 * is empty.
 */
void TcpServer::request_writer_loop()
{
    LOG_INFO("Request writer thread started.");

    while (true)
    {
        tcp_to_database_message_t request{};
        int result;

        /*
         * Lock the internal queue before checking its state.
         */
        result = pthread_mutex_lock(&request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to lock request queue mutex. Error: %d",
                result);

            break;
        }

        /*
         * Wait while the queue is empty and the application is still
         * running.
         *
         * pthread_cond_wait() atomically unlocks the mutex while the
         * thread is waiting and locks it again before returning.
         */
        while (request_queue_.empty() && running_)
        {
            result = pthread_cond_wait(&request_queue_condition_, &request_queue_mutex_);

            if (result != 0)
            {
                LOG_ERROR(
                    "Failed while waiting for request queue condition. "
                    "Error: %d",
                    result);

                pthread_mutex_unlock(&request_queue_mutex_);

                LOG_INFO("Request writer thread stopped.");

                return;
            }
        }

        /*
         * Stop only when shutdown was requested and there are no
         * remaining requests waiting to be forwarded.
         */
        if (!running_ && request_queue_.empty())
        {
            result = pthread_mutex_unlock(&request_queue_mutex_);

            if (result != 0)
            {
                LOG_ERROR(
                    "Failed to unlock request queue mutex. Error: %d",
                    result);
            }

            break;
        }

        request = request_queue_.front();
        request_queue_.pop();

        result = pthread_mutex_unlock(&request_queue_mutex_);

        if (result != 0)
        {
            LOG_ERROR(
                "Failed to unlock request queue mutex. Error: %d",
                result);

            break;
        }

        /*
         * Forward the request to Database through the existing
         * shared IPC queue.
         */
        if (!tcp_to_database_queue_->push(&request))
        {
            LOG_ERROR(
                "Failed to send request %llu to Database.",
                static_cast<unsigned long long>(
                    request.request_id));

            continue;
        }

        LOG_INFO(
            "Request %llu sent to Database.",
            static_cast<unsigned long long>(
                request.request_id));
    }

    LOG_INFO("Request writer thread stopped.");
}
