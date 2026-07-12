#ifndef BBG_CLIENT_H
#define BBG_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief BBG TCP client context.
 */
typedef struct
{
    /**
     * @brief TCP socket connected to the Parking System server.
     */
    int socket_fd;

    /**
     * @brief Parking System server IPv4 address.
     */
    char server_address[16];

    /**
     * @brief Parking System TCP server port.
     */
    uint16_t server_port;

} bbg_client_t;


bool bbg_client_initialize(bbg_client_t *client, const char *server_address, uint16_t server_port);

bool bbg_client_connect(bbg_client_t *client);

void bbg_client_disconnect(bbg_client_t *client);

#endif /* BBG_CLIENT_H */