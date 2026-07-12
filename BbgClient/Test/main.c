#include "bbg_client.h"
#include "tcp_test_protocol.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT    5000U

int main(void)
{
    bbg_client_t client;

    tcp_to_database_message_t request = {0};
    database_to_tcp_message_t response = {0};

    if (!bbg_client_initialize(
            &client,
            SERVER_ADDRESS,
            SERVER_PORT))
    {
        fprintf(stderr, "Failed to initialize BBG client.\n");

        return 1;
    }

    if (!bbg_client_connect(&client))
    {
        fprintf(stderr, "Failed to connect to TCP server.\n");

        return 1;
    }

    printf("Connected to TCP server.\n");

    /*
     * Prepare a simple parking request.
     */
    memset(&request, 0, sizeof(request));

    request.request_id = 1U;
    request.action = START_PARKING;

    strcpy(request.vehicle_number, "12-345-67");

    request.latitude = 15.0;
    request.longitude = 15.0;

    request.parking_start_time = (int64_t)time(NULL);
    request.parking_end_time = 0;

    /*
     * Send the request.
     */
    if (send(
            client.socket_fd,
            &request,
            sizeof(request),
            0) != sizeof(request))
    {
        fprintf(stderr, "Failed to send request.\n");

        bbg_client_disconnect(&client);

        return 1;
    }

    printf("Request sent.\n");

    /*
     * Wait for Database response.
     */
    if (recv(
            client.socket_fd,
            &response,
            sizeof(response),
            0) != sizeof(response))
    {
        fprintf(stderr, "Failed to receive response.\n");

        bbg_client_disconnect(&client);

        return 1;
    }

    printf("Response received.\n");

    printf("Request ID       : %llu\n",
           (unsigned long long)response.request_id);

    printf("Status           : %d\n",
           response.status);

    printf("Parking duration : %ld\n",
           response.parking_duration);

    printf("Parking cost     : %.2f\n",
           response.parking_cost);

    bbg_client_disconnect(&client);


    printf("Parking started successfully.\n");

    sleep(125);

     if (!bbg_client_initialize(
            &client,
            SERVER_ADDRESS,
            SERVER_PORT))
    {
        fprintf(stderr, "Failed to initialize BBG client.\n");

        return 1;
    }

    if (!bbg_client_connect(&client))
    {
        fprintf(stderr, "Failed to connect to TCP server.\n");

        return 1;
    }

    printf("Connected to TCP server.\n");

    request.request_id = 2;
    request.action = END_PARKING;

    strcpy(request.vehicle_number, "12-345-67");

    request.latitude = 15.0;
    request.longitude = 15.0;

    request.parking_start_time = 0;
    request.parking_end_time = (int64_t)time(NULL);

    /*
     * Send the request.
     */
    if (send(
            client.socket_fd,
            &request,
            sizeof(request),
            0) != sizeof(request))
    {
        fprintf(stderr, "Failed to send request.\n");

        bbg_client_disconnect(&client);

        return 1;
    }

    printf("Request sent.\n");

    /*
     * Wait for Database response.
     */
    if (recv(
            client.socket_fd,
            &response,
            sizeof(response),
            0) != sizeof(response))
    {
        fprintf(stderr, "Failed to receive response.\n");

        bbg_client_disconnect(&client);

        return 1;
    }

    printf("Response received.\n");

    printf("Request ID       : %llu\n",
           (unsigned long long)response.request_id);

    printf("Status           : %d\n",
           response.status);

    printf("Parking duration : %ld\n",
           response.parking_duration);

    printf("Parking cost     : %.2f\n",
           response.parking_cost);

    bbg_client_disconnect(&client);

    return 0;
}