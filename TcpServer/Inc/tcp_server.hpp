#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include "config.h"
#include "ipc_protocol.hpp"
#include "logger.h"
#include "shared_memory.hpp"
#include "shared_queue.hpp"

#include <cstddef>
#include <string>


/**
 * @brief TcpServer application.
 *
 * Connects to IPC resources created by the Database application
 * and later handles TCP client connections.
 */
class TcpServer
{
public:

    TcpServer();

    ~TcpServer();

    bool initialize();

    void run();

    bool shutdown();

private:

    bool load_configuration();

    bool attach_shared_memory();

    bool wait_for_queues();

    bool attach_queues();

    static void* request_writer_thread(void* argument);

    static void* response_reader_thread(void* argument);

    void request_writer_loop();

    void response_reader_loop();

    /**
     * @brief Shared memory object used for IPC with Database.
     */
    SharedMemory *shared_memory_;

    /**
     * @brief Queue sending requests from TcpServer to Database.
     */
    SharedQueue *tcp_to_database_queue_;

    /**
     * @brief Queue receiving responses from Database.
     */
    SharedQueue *database_to_tcp_queue_;

    /**
     * @brief Pointer to the IPC control header stored in shared memory.
     *
     * The header contains readiness flags used to synchronize
     * TcpServer and Database initialization.
     */
    ipc_header_t *ipc_header_;

    /**
     * @brief System V shared memory key.
     */
    key_t shared_memory_key_;

    /**
     * @brief Path to the TcpServer log file.
     */
    std::string log_file_;

    /**
     * @brief Enables or disables console logging.
     */
    bool enable_console_logging_;

    /**
     * @brief Identifier of the thread sending requests to Database.
     */
    pthread_t request_writer_thread_;

    /**
     * @brief Identifier of the thread receiving responses from Database.
     */
    pthread_t response_reader_thread_;

    /**
     * @brief Indicates whether TcpServer worker threads should continue running.
     */
    bool running_;
};

#endif /* TCP_SERVER_HPP */
