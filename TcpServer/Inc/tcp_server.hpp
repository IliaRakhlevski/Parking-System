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

    SharedMemory *shared_memory_;

    SharedQueue *tcp_to_database_queue_;

    SharedQueue *database_to_tcp_queue_;

    ipc_header_t *ipc_header_;

    key_t shared_memory_key_;

    std::string log_file_;

    bool enable_console_logging_;
};

#endif /* TCP_SERVER_HPP */
