#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include "config.h"
#include "ipc_protocol.hpp"
#include "logger.h"
#include "shared_memory.hpp"
#include "shared_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <netinet/in.h>
#include <unordered_map>


/**
 * @brief Information about a connected TCP client.
 */
struct tcp_client_t
{
    /**
     * @brief Client socket file descriptor.
     */
    int socket_fd;
};


/**
 * @brief TcpServer application.
 *
 * Accepts TCP client connections, converts client requests into IPC
 * requests and communicates with the Database application through
 * shared queues.
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

    static void* tcp_server_thread(void* argument);

    static void* request_writer_thread(void* argument);

    static void* response_reader_thread(void* argument);

    void tcp_server_loop();

    void request_writer_loop();

    void response_reader_loop();

    bool add_client(int client_socket);

    void remove_client(int client_socket);

    bool receive_client_request(int client_socket);

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
     * @brief Identifier of the thread accepting and serving TCP clients.
     */
    pthread_t tcp_server_thread_;

    /**
     * @brief Identifier of the thread sending requests to Database.
     */
    pthread_t request_writer_thread_;

    /**
     * @brief Identifier of the thread receiving responses from Database.
     */
    pthread_t response_reader_thread_;

    /**
     * @brief Internal queue containing client requests waiting to be sent
     *        to Database.
     */
    std::queue<tcp_to_database_message_t> request_queue_;

    /**
     * @brief Protects access to the internal request queue.
     */
    pthread_mutex_t request_queue_mutex_;

    /**
     * @brief Wakes the request writer thread when a new request is queued.
     */
    pthread_cond_t request_queue_condition_;

    /**
     * @brief Indicates whether the request queue mutex was initialized.
     */
    bool request_queue_mutex_initialized_;

    /**
     * @brief Indicates whether the request queue condition variable was
     *        initialized.
     */
    bool request_queue_condition_initialized_;

    /**
     * @brief Connected TCP clients indexed by socket file descriptor.
     */
    std::unordered_map<int, tcp_client_t> clients_;

    /**
     * @brief Pending requests indexed by request identifier.
     *
     * Each request identifier is associated with the socket descriptor
     * of the client that submitted the request.
     */
    std::unordered_map<std::uint64_t, int> pending_requests_;

    /**
     * @brief Protects the connected clients and pending requests tables.
     */
    pthread_mutex_t clients_mutex_;

    /**
     * @brief Indicates whether the clients mutex was initialized.
     */
    bool clients_mutex_initialized_;

    /**
     * @brief Indicates whether TcpServer worker threads should continue
     *        running.
     */
    bool running_;

    /**
     * @brief TCP server listening socket.
     *
     * This socket is used to accept incoming client connections.
     */
    int server_socket_;

    /**
     * @brief TCP port used by the server.
     */
    std::uint16_t server_port_;

    /**
     * @brief Maximum number of pending client connections.
     *
     * Specifies the maximum length of the operating system's
     * pending connection queue passed to listen().
     */
    int listen_backlog_;

};

#endif /* TCP_SERVER_HPP */