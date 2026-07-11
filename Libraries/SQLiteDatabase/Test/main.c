#include "sqlite_database.h"
#include "logger.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>

int main(void)
{
    sqlite_database_t database = {0};

    int city_id;
    char city[64];

    double min_latitude;
    double max_latitude;
    double min_longitude;
    double max_longitude;
    double price_per_minute;

    int64_t start_time;
    int64_t end_time;
    double parking_cost;

    if (logger_init("sqlite_database.log", 1) != 0)
    {
        return -1;
    }

    if (sqlite_database_open(&database, "parking.db") != 0)
    {
        logger_close();
        return -1;
    }

    if (sqlite_database_initialize(&database) != 0)
    {
        sqlite_database_close(&database);
        logger_close();
        return -1;
    }

    sqlite_database_add_city(
        &database,
        "Eilat",
        40.0,
        49.9,
        40.0,
        49.9,
        0.75);

    sqlite_database_update_city_price(
        &database,
        "Eilat",
        0.90);

    if (sqlite_database_find_city(
            &database,
            45.0,
            45.0,
            &city_id) == 0)
    {
        printf("City ID: %d\n", city_id);

        if (sqlite_database_get_city(
                &database,
                city_id,
                city,
                sizeof(city),
                &min_latitude,
                &max_latitude,
                &min_longitude,
                &max_longitude,
                &price_per_minute) == 0)
        {
            printf("\n");
            printf("City: %s\n", city);
            printf("Latitude: %.2f - %.2f\n",
                   min_latitude,
                   max_latitude);
            printf("Longitude: %.2f - %.2f\n",
                   min_longitude,
                   max_longitude);
            printf("Price: %.2f\n",
                   price_per_minute);
        }
    }

    sqlite_database_delete_city(
        &database,
        "Eilat");


    start_time = (int64_t)time(NULL);
    /* Find the city by coordinates. */
    if (sqlite_database_find_city(
            &database,
            15.0,
            15.0,
            &city_id) == 0)
    {
        printf("City ID: %d\n", city_id);

        if (sqlite_database_get_city(
                &database,
                city_id,
                city,
                sizeof(city),
                &min_latitude,
                &max_latitude,
                &min_longitude,
                &max_longitude,
                &price_per_minute) == 0)
        {
            printf("\n");
            printf("City: %s\n", city);
            printf("Price: %.2f\n", price_per_minute);
        }

        start_time = (int64_t)time(NULL);
        end_time = start_time + 95;

        if (sqlite_database_start_parking(
                &database,
                "123-45-678",
                15.0,
                15.0,
                city_id,
                start_time) == 0)
        {
            printf("\nParking started.\n");

            if (sqlite_database_stop_parking(
                    &database,
                    "123-45-678",
                    end_time,
                    &parking_cost) == 0)
            {
                printf("Parking stopped.\n");
                printf("Parking cost: %.2f\n", parking_cost);
            }
        }
    }

    sqlite_database_close(&database);

    logger_close();

    return 0;
}

