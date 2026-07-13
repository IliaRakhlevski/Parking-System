#ifndef PARKING_PROTOCOL_H
#define PARKING_PROTOCOL_H

#include <stdint.h>

#define VEHICLE_NUMBER_SIZE 16
#define CITY_NAME_SIZE      32

#ifdef __cplusplus

/**
 * @brief Parking request action.
 */
enum class parking_action_t : uint32_t
{
    START_PARKING = 0U,
    END_PARKING,
    ACKNOWLEDGE
};

/**
 * @brief Parking request processing status.
 */
enum class ipc_status_t : uint32_t
{
    SUCCESS = 0U,
    INVALID_REQUEST,
    VEHICLE_NOT_FOUND,
    DATABASE_ERROR,
    INTERNAL_ERROR
};

#else

/**
 * @brief Parking request action type.
 */
typedef uint32_t parking_action_t;

enum
{
    START_PARKING = 0U,
    END_PARKING,
    ACKNOWLEDGE
};

/**
 * @brief Parking request processing status type.
 */
typedef uint32_t ipc_status_t;

enum
{
    IPC_STATUS_SUCCESS = 0U,
    IPC_STATUS_INVALID_REQUEST,
    IPC_STATUS_VEHICLE_NOT_FOUND,
    IPC_STATUS_DATABASE_ERROR,
    IPC_STATUS_INTERNAL_ERROR
};

#endif /* __cplusplus */

/**
 * @brief Message transferred from TcpServer to Database.
 */
typedef struct
{
    uint64_t request_id;

    parking_action_t action;

    char vehicle_number[VEHICLE_NUMBER_SIZE];
    char city[CITY_NAME_SIZE];

    double latitude;
    double longitude;

    int64_t parking_start_time;
    int64_t parking_end_time;

} tcp_to_database_message_t;

/**
 * @brief Message transferred from Database to TcpServer.
 */
typedef struct
{
    uint64_t request_id;

    parking_action_t action;
    ipc_status_t status;

    char vehicle_number[VEHICLE_NUMBER_SIZE];

    int64_t parking_start_time;
    int64_t parking_end_time;
    int64_t parking_duration;

    double parking_cost;

} database_to_tcp_message_t;

#endif /* PARKING_PROTOCOL_H */
