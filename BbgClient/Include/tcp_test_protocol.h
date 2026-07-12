#ifndef TCP_TEST_PROTOCOL_H
#define TCP_TEST_PROTOCOL_H

#include <stdint.h>

#define VEHICLE_NUMBER_SIZE 16
#define CITY_NAME_SIZE      32

typedef enum
{
    START_PARKING = 0,
    END_PARKING,
    ACKNOWLEDGE
} parking_action_t;

typedef enum
{
    IPC_STATUS_SUCCESS = 0,
    IPC_STATUS_FAILURE
} ipc_status_t;

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

#endif /* TCP_TEST_PROTOCOL_H */