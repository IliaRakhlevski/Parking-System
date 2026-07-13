#include "bbg_client.h"

#include "config.h"
#include "logger.h"
#include "parking_protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>


/**
 * @brief Maximum size of a file path stored by the BBG client.
 */
#define BBG_CLIENT_FILE_PATH_SIZE 256U

/**
 * @brief BBG client configuration.
 *
 * This structure stores all configuration parameters loaded
 * from the BBG client configuration file.
 */
typedef struct
{
    /**
     * @brief Parking System TCP server IPv4 address.
     */
    char server_address[BBG_CLIENT_IPV4_ADDRESS_SIZE];

    /**
     * @brief Parking System TCP server port.
     */
    uint16_t server_port;

    /**
     * @brief TCP process log file path.
     */
    char tcp_log_file[BBG_CLIENT_FILE_PATH_SIZE];

    /**
     * @brief I2C process log file path.
     */
    char i2c_log_file[BBG_CLIENT_FILE_PATH_SIZE];

    /**
     * @brief Enable or disable console logging.
     *
     * Value:
     *      - 0 Console logging disabled.
     *      - 1 Console logging enabled.
     */
    int console_logging;

} bbg_client_config_t;

static bbg_client_config_t bbg_config;


/**
 * @brief Read end of the unnamed pipe.
 */
#define PIPE_READ_END   0

/**
 * @brief Write end of the unnamed pipe.
 */
#define PIPE_WRITE_END  1


/**
 * @brief Connect to the Parking System TCP server.
 *
 * Creates a TCP socket and establishes a connection to the
 * Parking System server using the configuration loaded from
 * the BBG client configuration file.
 *
 * @return
 *      - TCP socket descriptor on success.
 *      - -1 on failure.
 */
static int bbg_client_connect(void)
{
    int socket_fd;
    struct sockaddr_in server_address;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd == -1)
    {
        LOG_ERROR("Failed to create TCP socket.");

        return -1;
    }

    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(bbg_config.server_port);

    if (inet_pton(AF_INET,
                  bbg_config.server_address,
                  &server_address.sin_addr) != 1)
    {
        LOG_ERROR("Invalid TCP server IPv4 address.");

        close(socket_fd);

        return -1;
    }

    if (connect(socket_fd,
                (struct sockaddr *)&server_address,
                sizeof(server_address)) == -1)
    {
        LOG_ERROR("Failed to connect to TCP server.");

        close(socket_fd);

        return -1;
    }

    LOG_INFO("Connected to TCP server.");

    return socket_fd;
}

/**
 * @brief Disconnect from the Parking System TCP server.
 *
 * Closes the specified TCP socket.
 *
 * @param socket_fd Connected TCP socket descriptor.
 */
static void bbg_client_disconnect(int socket_fd)
{
    if (socket_fd != -1)
    {
        close(socket_fd);

        LOG_INFO("Disconnected from TCP server.");
    }
}

/**
 * @brief Converts IPC status to a human-readable string.
 *
 * This function is intended for logging and debugging purposes.
 *
 * @param status IPC response status.
 *
 * @return
 *         - "SUCCESS"              Operation completed successfully.
 *         - "INVALID_REQUEST"      Invalid request received.
 *         - "VEHICLE_NOT_FOUND"    Vehicle was not found.
 *         - "DATABASE_ERROR"       Database processing error.
 *         - "INTERNAL_ERROR"       Internal processing error.
 *         - "UNKNOWN_STATUS"       Unknown IPC status.
 */
static const char *ipc_status_to_string(ipc_status_t status)
{
    switch (status)
    {
        case IPC_STATUS_SUCCESS:
            return "SUCCESS";

        case IPC_STATUS_INVALID_REQUEST:
            return "INVALID_REQUEST";

        case IPC_STATUS_VEHICLE_NOT_FOUND:
            return "VEHICLE_NOT_FOUND";

        case IPC_STATUS_DATABASE_ERROR:
            return "DATABASE_ERROR";

        case IPC_STATUS_INTERNAL_ERROR:
            return "INTERNAL_ERROR";

        default:
            return "UNKNOWN_STATUS";
    }
}

/**
 * @brief Internal TCP process entry point.
 *
 * Reads parking requests from the unnamed pipe, sends them to the
 * Parking System TCP server and receives the corresponding responses.
 *
 * @param pipe_read_fd Read side of the unnamed pipe.
 *
 * @return
 *      - 0  Success.
 *      - -1 Failure.
 */
static int tcp_process_run(int pipe_read_fd)
{
    tcp_to_database_message_t request;
    database_to_tcp_message_t response;

    ssize_t bytes_read;
    int socket_fd;

    if (logger_init(bbg_config.tcp_log_file, bbg_config.console_logging) != 0)
    {
        fprintf(stderr, "Failed to initialize TCP logger.\n");

        close(pipe_read_fd);

        return -1;
    }

    LOG_INFO("TCP process started.");

    while (1)
    {
        bytes_read = read(pipe_read_fd, &request, sizeof(request));

        if (bytes_read == 0)
        {
            LOG_INFO("I2C process closed the pipe.");
            break;
        }

        if (bytes_read == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LOG_ERROR("Failed to read request from pipe.");
            break;
        }

        if (bytes_read != (ssize_t)sizeof(request))
        {
            LOG_ERROR("Incomplete request received from pipe.");
            break;
        }

        LOG_INFO("Request %llu received from I2C process.",
                 (unsigned long long)request.request_id);

        socket_fd = bbg_client_connect();

        if (socket_fd == -1)
        {
            LOG_ERROR("Failed to connect to TCP server.");
            continue;
        }

        if (send(socket_fd,
                 &request,
                 sizeof(request),
                 0) != (ssize_t)sizeof(request))
        {
            LOG_ERROR("Failed to send request %llu.",
                      (unsigned long long)request.request_id);

            bbg_client_disconnect(socket_fd);
            continue;
        }

        if (recv(socket_fd,
                 &response,
                 sizeof(response),
                 0) != (ssize_t)sizeof(response))
        {
            LOG_ERROR("Failed to receive response for request %llu.",
                      (unsigned long long)request.request_id);

            bbg_client_disconnect(socket_fd);
            continue;
        }

        if (request.action == START_PARKING)
        {
            LOG_INFO("START_PARKING response received. Request ID: %llu, vehicle: %s, status: %s.",
                    (unsigned long long)response.request_id,
                    response.vehicle_number,
                    ipc_status_to_string(response.status));
        }
        else if (request.action == END_PARKING)
        {
            LOG_INFO("END_PARKING response received. Request ID: %llu, vehicle: %s, status: %s, duration: %lld seconds, cost: %.2f.",
                    (unsigned long long)response.request_id,
                    response.vehicle_number,
                    ipc_status_to_string(response.status),
                    (long long)response.parking_duration,
                    response.parking_cost);
        }
        else
        {
            LOG_ERROR("Unknown response action received. Request ID: %llu.",
                    (unsigned long long)response.request_id);
        }

        bbg_client_disconnect(socket_fd);
    }

    LOG_INFO("TCP process stopped.");

    logger_close();
    close(pipe_read_fd);

    return 0;
}

/**
 * @brief Test parking event.
 */
typedef struct
{
    /**
     * @brief Parking action.
     */
    parking_action_t action;

    /**
     * @brief Vehicle registration number.
     */
    const char *vehicle_number;

    /**
     * @brief City name.
     */
    const char *city;

    /**
     * @brief Latitude.
     */
    double latitude;

    /**
     * @brief Longitude.
     */
    double longitude;

    /**
     * @brief Delay before sending the next event, seconds.
     */
    uint32_t delay_seconds;

} test_event_t;

static const test_event_t test_events[] =
{
    { START_PARKING, "12-345-67", "Tel Aviv",  15.0, 15.0, 5 },
    { START_PARKING, "98-765-43", "Haifa",     25.0, 25.0, 4 },
    { START_PARKING, "11-222-33", "Jerusalem", 35.0, 35.0, 3 },
    { START_PARKING, "44-111-22", "Tel Aviv",  15.0, 15.0, 2 },
    { START_PARKING, "55-222-33", "Haifa",     25.0, 25.0, 3 },
    { START_PARKING, "66-333-44", "Jerusalem", 35.0, 35.0, 4 },

    { END_PARKING,   "98-765-43", "Haifa",     25.0, 25.0, 2 },
    { END_PARKING,   "12-345-67", "Tel Aviv",  15.0, 15.0, 2 },

    { START_PARKING, "77-444-55", "Tel Aviv",  15.0, 15.0, 3 },
    { START_PARKING, "88-555-66", "Haifa",     25.0, 25.0, 2 },

    { END_PARKING,   "11-222-33", "Jerusalem", 35.0, 35.0, 2 },
    { END_PARKING,   "44-111-22", "Tel Aviv",  15.0, 15.0, 2 },

    { START_PARKING, "99-666-77", "Jerusalem", 35.0, 35.0, 3 },

    { END_PARKING,   "55-222-33", "Haifa",     25.0, 25.0, 2 },
    { END_PARKING,   "66-333-44", "Jerusalem", 35.0, 35.0, 2 },
    { END_PARKING,   "77-444-55", "Tel Aviv",  15.0, 15.0, 2 },
    { END_PARKING,   "88-555-66", "Haifa",     25.0, 25.0, 2 },
    { END_PARKING,   "99-666-77", "Jerusalem", 35.0, 35.0, 2 },
};

#define TEST_EVENTS_COUNT (sizeof(test_events) / sizeof(test_events[0]))

/**
 * @brief Generate a test parking event.
 *
 * @param pipe_write_fd Write side of the unnamed pipe.
 *
 * @return
 *      - 0  Success.
 *      - -1 Failure.
 */
static int generate_test_message(int pipe_write_fd)
{
    static size_t current_event = 0;
    static uint64_t request_id = 1;

    tcp_to_database_message_t message;

    if (current_event >= (sizeof(test_events) / sizeof(test_events[0])))
    {
        return -1;
    }

    memset(&message, 0, sizeof(message));

    message.request_id = request_id++;
    message.action = test_events[current_event].action;

    strncpy(message.vehicle_number,
            test_events[current_event].vehicle_number,
            VEHICLE_NUMBER_SIZE - 1);

    strncpy(message.city,
            test_events[current_event].city,
            CITY_NAME_SIZE - 1);

    message.latitude = test_events[current_event].latitude;
    message.longitude = test_events[current_event].longitude;

    if (message.action == START_PARKING)
    {
        message.parking_start_time = (int64_t)time(NULL);
        message.parking_end_time = 0;
    }
    else if (message.action == END_PARKING)
    {
        message.parking_start_time = 0;
        message.parking_end_time = (int64_t)time(NULL);
    }
    else
    {
        LOG_ERROR("Invalid test parking action.");

        return -1;
    }

    if (write(pipe_write_fd, &message, sizeof(message)) != sizeof(message))
    {
        LOG_ERROR("Failed to write test message to pipe.");

        return -1;
    }

    LOG_INFO("Generated %s request %llu for vehicle %s.",
             (message.action == START_PARKING) ? "START_PARKING" : "END_PARKING",
             (unsigned long long)message.request_id,
             message.vehicle_number);

    sleep(test_events[current_event].delay_seconds);

    current_event = (current_event + 1U) % TEST_EVENTS_COUNT;

    return 0;
}

/**
 * @brief Internal I2C process entry point.
 *
 * @param pipe_write_fd Write side of the unnamed pipe.
 *
 * @return
 *      - 0  Success.
 *      - -1 Failure.
 */
static int i2c_process_run(int pipe_write_fd)
{
    if (logger_init(bbg_config.i2c_log_file, bbg_config.console_logging) != 0)
    {
        fprintf(stderr, "Failed to initialize I2C logger.\n");

        close(pipe_write_fd);

        return -1;
    }

    LOG_INFO("I2C process started.");

    while (1)
    {
        if (generate_test_message(pipe_write_fd) != 0)
        {
            break;
        }

        sleep(1);
    }

    LOG_INFO("I2C process stopped.");

    logger_close();

    close(pipe_write_fd);

    return 0;
}

/**
 * @brief Load BBG client configuration.
 *
 * Reads all required configuration parameters from the
 * configuration file representation and stores them in
 * the BBG client configuration structure.
 *
 * @param config Loaded configuration object.
 * @param bbg_config BBG client configuration.
 *
 * @return
 *      - 0  Configuration loaded successfully.
 *      - -1 Invalid argument or configuration error.
 */
static int load_bbg_client_configuration(const config_t *config, bbg_client_config_t *bbg_config)
{
     if ((config == NULL) || (bbg_config == NULL))
    {
        return -1;
    }

    memset(bbg_config, 0, sizeof(*bbg_config));

    strncpy(bbg_config->server_address,
        config_get_string(
            config,
            "server_address",
            "127.0.0.1"),
        sizeof(bbg_config->server_address) - 1U);

    bbg_config->server_address[sizeof(bbg_config->server_address) - 1U] = '\0';

    bbg_config->server_port =
        (uint16_t)config_get_int(
            config,
            "server_port",
            5000);

    strncpy(
        bbg_config->tcp_log_file,
        config_get_string(
            config,
            "tcp_log_file",
            "../Logs/bbg_tcp.log"),
        sizeof(bbg_config->tcp_log_file) - 1U);

    bbg_config->tcp_log_file[sizeof(bbg_config->tcp_log_file) - 1U] = '\0';

    strncpy(
        bbg_config->i2c_log_file,
        config_get_string(
            config,
            "i2c_log_file",
            "../Logs/bbg_i2c.log"),
        sizeof(bbg_config->i2c_log_file) - 1U);

    bbg_config->i2c_log_file[sizeof(bbg_config->i2c_log_file) - 1U] = '\0';

    bbg_config->console_logging =
        config_get_int(
            config,
            "console_logging",
            1);

    return 0;
}


/**
 * @brief Start the BBG client application.
 *
 * @param config_file_path Path to the BBG client configuration file.
 *
 * @return
 *      - 0  Success.
 *      - -1 Failure.
 */
int bbg_client_run(const char *config_file_path)
{
    config_t config;

    int pipe_fd[2];
    pid_t process_id;

    if (config_file_path == NULL)
    {
        fprintf(stderr, "BBG client configuration file path is NULL.\n");

        return -1;
    }

    if (config_load(&config, config_file_path) != 0)
    {
        fprintf(stderr, "Failed to load BBG client configuration: %s\n", config_file_path);

        return -1;
    }

    if (load_bbg_client_configuration(&config, &bbg_config) != 0)
    {
        fprintf(stderr, "Failed to read BBG client configuration.\n");

        return -1;
    }

    if (pipe(pipe_fd) == -1)
    {
        fprintf(stderr, "Failed to create pipe.\n");
        return -1;
    }

    process_id = fork();

    if (process_id == -1)
    {
        fprintf(stderr, "Failed to create child process.\n");

        close(pipe_fd[PIPE_READ_END]);
        close(pipe_fd[PIPE_WRITE_END]);

        return -1;
    }

    if (process_id == 0)
    {
        close(pipe_fd[PIPE_READ_END]);

        return i2c_process_run(pipe_fd[PIPE_WRITE_END]);
    }

    close(pipe_fd[PIPE_WRITE_END]);

    return tcp_process_run(pipe_fd[PIPE_READ_END]);
}
