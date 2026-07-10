#ifndef IPC_PROTOCOL_HPP
#define IPC_PROTOCOL_HPP

#include "shared_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

/**
 * @brief Align a value up to the specified alignment.
 *
 * @param value Value to align.
 * @param alignment Required alignment in bytes.
 *
 * @return Aligned value.
 */
constexpr std::size_t align_up(std::size_t value, std::size_t alignment)
{
    return ((value + alignment - 1U) / alignment) * alignment;
}

/**
 * @brief Maximum time to wait for the shared memory segment, in milliseconds.
 */
constexpr std::int64_t IPC_ATTACH_TIMEOUT_MS = 10000U;

/**
 * @brief Delay between IPC initialization checks, in milliseconds.
 */
constexpr std::size_t IPC_RETRY_DELAY_MS = 100U;

/**
 * @brief Default configuration file path.
 */
constexpr const char *DEFAULT_CONFIG_FILE =
    "../Config/parking_system.conf";

/**
 * @brief Default database file path.
 */
constexpr const char *DEFAULT_DATABASE_FILE =
    "../Database/parking.db";

/**
 * @brief Default log file path.
 */
constexpr const char *DEFAULT_DATABASE_LOG_FILE =
    "../Logs/database.log";

/**
 * @brief Default path to the TcpServer log file.
 */
constexpr const char *DEFAULT_TCP_SERVER_LOG_FILE =
    "../Logs/tcp_server.log";

/**
 * @brief Default console logging state.
 */
constexpr bool DEFAULT_ENABLE_CONSOLE_LOGGING = true;

/**
 * @brief Maximum length of a vehicle registration number,
 * including the null terminator.
 */
constexpr std::size_t VEHICLE_NUMBER_SIZE = 16U;

/**
 * @brief Maximum length of a city name,
 * including the null terminator.
 */
constexpr std::size_t CITY_NAME_SIZE = 32U;

/**
 * @brief Default System V shared memory key.
 *
 * This key is used if no value is provided
 * in the configuration file.
 */
constexpr key_t IPC_SHARED_MEMORY_KEY = 0x1234;

/**
 * @brief IPC resource readiness state.
 */
enum class ipc_ready_state_t : std::uint32_t
{
    NOT_READY = 0U,
    READY = 1U
};

/**
 * @brief Parking operation.
 */
enum class parking_action_t : std::uint32_t
{
    START_PARKING = 0U,
    END_PARKING,
    ACKNOWLEDGE
};

/**
 * @brief IPC request processing status.
 */
enum class ipc_status_t : std::uint32_t
{
    SUCCESS = 0U,
    INVALID_REQUEST,
    VEHICLE_NOT_FOUND,
    DATABASE_ERROR,
    INTERNAL_ERROR
};

/**
 * @brief Shared memory header.
 */
struct ipc_header_t
{
    ipc_ready_state_t tcp_to_database_queue_ready;
    ipc_ready_state_t database_to_tcp_queue_ready;
};

/**
 * @brief Message transferred from TcpServer to Database.
 */
struct tcp_to_database_message_t
{
    std::uint64_t request_id;

    parking_action_t action;

    char vehicle_number[VEHICLE_NUMBER_SIZE];
    char city[CITY_NAME_SIZE];

    double latitude;
    double longitude;

    std::int64_t parking_start_time;
    std::int64_t parking_end_time;

    double parking_cost;
};

/**
 * @brief Message transferred from Database to TcpServer.
 */
struct database_to_tcp_message_t
{
    std::uint64_t request_id;

    parking_action_t action;
    ipc_status_t status;

    char vehicle_number[VEHICLE_NUMBER_SIZE];

    std::int64_t parking_start_time;
    std::int64_t parking_end_time;

    std::int64_t parking_duration;

    double parking_cost;
};

/**
 * @brief Maximum number of elements in the
 * TcpServer-to-Database queue.
 */
constexpr std::size_t TCP_TO_DATABASE_QUEUE_CAPACITY = 32U;

/**
 * @brief Maximum number of elements in the
 * Database-to-TcpServer queue.
 */
constexpr std::size_t DATABASE_TO_TCP_QUEUE_CAPACITY = 32U;

/**
 * @brief POSIX semaphore used by the
 * TcpServer-to-Database queue.
 */
constexpr const char *TCP_TO_DATABASE_SEMAPHORE_NAME =
    "/parking_system_tcp_to_database_queue";

/**
 * @brief POSIX semaphore used by the
 * Database-to-TcpServer queue.
 */
constexpr const char *DATABASE_TO_TCP_SEMAPHORE_NAME =
    "/parking_system_database_to_tcp_queue";

/**
 * @brief Offset of the TcpServer-to-Database queue.
 */
constexpr std::size_t TCP_TO_DATABASE_QUEUE_OFFSET =
    align_up(sizeof(ipc_header_t),
             alignof(queue_header_t));

/**
 * @brief Total size of the TcpServer-to-Database queue.
 */
constexpr std::size_t TCP_TO_DATABASE_QUEUE_SIZE =
    sizeof(queue_header_t) +
    TCP_TO_DATABASE_QUEUE_CAPACITY *
        sizeof(tcp_to_database_message_t);

/**
 * @brief Offset of the Database-to-TcpServer queue.
 */
constexpr std::size_t DATABASE_TO_TCP_QUEUE_OFFSET =
    align_up(TCP_TO_DATABASE_QUEUE_OFFSET +
                 TCP_TO_DATABASE_QUEUE_SIZE,
             alignof(queue_header_t));

/**
 * @brief Total size of the Database-to-TcpServer queue.
 */
constexpr std::size_t DATABASE_TO_TCP_QUEUE_SIZE =
    sizeof(queue_header_t) +
    DATABASE_TO_TCP_QUEUE_CAPACITY *
        sizeof(database_to_tcp_message_t);

/**
 * @brief Total size of the shared memory segment.
 */
constexpr std::size_t IPC_SHARED_MEMORY_SIZE =
    DATABASE_TO_TCP_QUEUE_OFFSET +
    DATABASE_TO_TCP_QUEUE_SIZE;

#endif /* IPC_PROTOCOL_HPP */