#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "config.h"
#include "ipc_protocol.hpp"
#include "logger.h"
#include "shared_memory.hpp"
#include "shared_queue.hpp"
#include <string>

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
};


#endif /* DATABASE_HPP */