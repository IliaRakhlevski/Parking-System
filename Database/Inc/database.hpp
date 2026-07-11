#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "config.h"
#include "ipc_protocol.hpp"
#include "logger.h"
#include "shared_memory.hpp"
#include "shared_queue.hpp"
#include <string>
#include <queue>


class Database
{
public:
    Database();

    ~Database();

    bool initialize();

    void run();
    
    bool shutdown();

private:

    bool load_configuration();

    bool initialize_shared_memory();

    bool initialize_queues();

    static void* request_reader_thread(void* argument);

    static void* response_writer_thread(void* argument);

    void request_reader_loop();

    void response_writer_loop();

    /**
     * @brief Shared memory object.
     */
    SharedMemory *shared_memory_;

    /**
     * @brief Queue receiving requests from TcpServer.
     */
    SharedQueue *tcp_to_database_queue_;

    /**
     * @brief Queue sending responses to TcpServer.
     */
    SharedQueue *database_to_tcp_queue_;

    /**
     * @brief Pointer to the shared memory header.
     */
    ipc_header_t *ipc_header_;

    /**
     * @brief System V shared memory key.
     */
    key_t shared_memory_key_;

    /**
     * @brief Shared memory segment size.
     */
    std::size_t shared_memory_size_;

    /**
     * @brief Path to the SQLite database file.
     */
    std::string database_file_;

    /**
     * @brief Path to the log file.
     */
    std::string log_file_;

    /**
     * @brief Enable console logging.
     */
    bool enable_console_logging_;

    /**
     * @brief Path to the SQLite database file.
     */
    std::string database_path_;


    /**
     * @brief Request reader thread identifier.
     *
     * Reads requests from the TcpServer-to-Database shared queue
     * and places them into the internal Database request queue.
     */
    pthread_t request_reader_thread_;

    /**
     * @brief Response writer thread identifier.
     *
     * Waits for requests in the internal Database queue,
     * processes them and sends responses to TcpServer.
     */
    pthread_t response_writer_thread_;

    /**
     * @brief Indicates whether worker threads should continue running.
     *
     * This flag is checked by all worker threads and is cleared
     * during application shutdown to terminate their main loops.
     */
    bool running_;

    /**
     * @brief Internal queue of pending Database requests.
     *
     * Requests received from the shared queue are temporarily stored
     * here until they are processed by the response writer thread.
     */
    std::queue<tcp_to_database_message_t> request_queue_;

    /**
     * @brief Protects access to the internal request queue.
     */
    pthread_mutex_t request_queue_mutex_;

    /**
     * @brief Signals that new requests are available.
     *
     * Used by the request reader thread to wake up the response
     * writer thread whenever a new request is added to the queue.
     */
    pthread_cond_t request_queue_condition_;

    /**
     * @brief Indicates whether the request queue mutex was initialized.
     */
    bool request_queue_mutex_initialized_;

    /**
     * @brief Indicates whether the request queue condition variable was initialized.
     */
    bool request_queue_condition_initialized_;
};


#endif /* DATABASE_HPP */