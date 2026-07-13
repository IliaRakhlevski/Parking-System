#ifndef BBG_CLIENT_H
#define BBG_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file bbg_client.h
 * @brief Public interface of the BeagleBone Green client application.
 */

/**
 * @brief Maximum size of an IPv4 address string, including
 *        the terminating null character.
 */
#define BBG_CLIENT_IPV4_ADDRESS_SIZE 16U


int bbg_client_run(const char *config_file_path);


#endif /* BBG_CLIENT_H */