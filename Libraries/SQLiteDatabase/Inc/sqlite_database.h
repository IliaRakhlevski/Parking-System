#ifndef SQLITE_DATABASE_H
#define SQLITE_DATABASE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief SQLite database object.
 */
typedef struct
{
    /**
     * @brief SQLite database handle.
     */
    sqlite3* database;

} sqlite_database_t;


int sqlite_database_open(sqlite_database_t* database, const char* database_file);

void sqlite_database_close(sqlite_database_t* database);

int sqlite_database_initialize(sqlite_database_t* database);

int sqlite_database_add_city(sqlite_database_t* database, const char* city, double min_latitude, double max_latitude, double min_longitude,
                                double max_longitude, double price_per_minute);

int sqlite_database_update_city_price(sqlite_database_t* database, const char* city, double price_per_minute);

int sqlite_database_delete_city(sqlite_database_t* database, const char* city);

int sqlite_database_find_city(sqlite_database_t* database, double latitude, double longitude, int* city_id);

int sqlite_database_get_city(sqlite_database_t* database, int city_id, char* city, size_t city_size,
                                double* min_latitude, double* max_latitude, double* min_longitude, double* max_longitude, double* price_per_minute);

int sqlite_database_start_parking(sqlite_database_t* database, const char* vehicle_number, double latitude,
                                double longitude, int city_id, int64_t start_time);

int sqlite_database_stop_parking(sqlite_database_t* database, const char* vehicle_number, int64_t end_time, int64_t* start_time, double* cost);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE_DATABASE_H */
