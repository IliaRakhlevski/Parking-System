#include "bbg_client.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief Initialize the BBG client.
 */
bool bbg_client_initialize(
    bbg_client_t *client,
    const char *server_address,
    uint16_t server_port)
{
    if ((client == NULL) || (server_address == NULL))
    {
        return false;
    }

    memset(client, 0, sizeof(*client));

    client->socket_fd = -1;
    client->server_port = server_port;

    strncpy(
        client->server_address,
        server_address,
        sizeof(client->server_address) - 1U);

    client->server_address[
        sizeof(client->server_address) - 1U] = '\0';

    return true;
}

/**
 * @brief Connect to the Parking System TCP server.
 */
bool bbg_client_connect(
    bbg_client_t *client)
{
    struct sockaddr_in server_address = {0};

    if (client == NULL)
    {
        return false;
    }

    client->socket_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0);

    if (client->socket_fd == -1)
    {
        return false;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(client->server_port);

    if (inet_pton(
            AF_INET,
            client->server_address,
            &server_address.sin_addr) != 1)
    {
        close(client->socket_fd);
        client->socket_fd = -1;

        return false;
    }

    if (connect(
            client->socket_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)) == -1)
    {
        close(client->socket_fd);
        client->socket_fd = -1;

        return false;
    }

    return true;
}

/**
 * @brief Disconnect from the Parking System TCP server.
 */
void bbg_client_disconnect(
    bbg_client_t *client)
{
    if (client == NULL)
    {
        return;
    }

    if (client->socket_fd != -1)
    {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
}