#include "sqlite_database.h"
#include "logger.h"
#include <stddef.h>
#include <stdio.h>


/**
 * @brief Opens the SQLite database.
 *
 * Opens the specified SQLite database file. If the file does not exist,
 * SQLite creates it automatically.
 *
 * @param database Pointer to the SQLite database object.
 * @param database_file Path to the SQLite database file.
 *
 * @return
 *      - 0  Success.
 *      - -1 Invalid argument, database already open, or SQLite error.
 */
int sqlite_database_open(sqlite_database_t* database, const char* database_file)
{
    int result;

    if (database == NULL)
    {
        LOG_ERROR("SQLite database object is NULL.");
        return -1;
    }

    if (database_file == NULL)
    {
        LOG_ERROR("SQLite database file path is NULL.");
        return -1;
    }

    if (database->database != NULL)
    {
        LOG_ERROR("SQLite database is already open.");
        return -1;
    }

    result = sqlite3_open(database_file, &database->database);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to open SQLite database '%s': %s.",
            database_file,
            sqlite3_errmsg(database->database));

        sqlite3_close(database->database);
        database->database = NULL;

        return -1;
    }

    LOG_INFO(
        "SQLite database '%s' opened successfully.",
        database_file);

    return 0;
}

/**
 * @brief Closes the SQLite database.
 *
 * Releases the SQLite connection associated with the database object.
 * If the database is already closed, the function does nothing.
 *
 * @param database Pointer to the SQLite database object.
 */
void sqlite_database_close(sqlite_database_t* database)
{
    int result;

    if (database == NULL)
    {
        LOG_ERROR("SQLite database object is NULL.");
        return;
    }

    if (database->database == NULL)
    {
        return;
    }

    result = sqlite3_close(database->database);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to close SQLite database: %s.",
            sqlite3_errmsg(database->database));

        return;
    }

    database->database = NULL;

    LOG_INFO("SQLite database closed successfully.");
}

/**
 * @brief Initializes the SQLite database schema.
 *
 * This function prepares the database for use by removing the existing
 * tables and creating a new empty database schema.
 *
 * During development, the previous database contents are discarded
 * every time this function is called.
 *
 * @param database Pointer to the SQLite database object.
 *
 * @return
 *      - 0  Success.
 *      - -1 Initialization failed.
 */
int sqlite_database_initialize(sqlite_database_t* database)
{
    char* error_message = NULL;
    int result;

    const char* initialize_sql =
        "BEGIN TRANSACTION;"

        "DROP TABLE IF EXISTS parking_sessions;"
        "DROP TABLE IF EXISTS parking_cities;"

        "CREATE TABLE parking_cities ("
        "    id INTEGER PRIMARY KEY,"
        "    city TEXT NOT NULL UNIQUE,"
        "    min_latitude REAL NOT NULL,"
        "    max_latitude REAL NOT NULL,"
        "    min_longitude REAL NOT NULL,"
        "    max_longitude REAL NOT NULL,"
        "    price_per_minute REAL NOT NULL CHECK (price_per_minute >= 0),"
        "    CHECK (min_latitude < max_latitude),"
        "    CHECK (min_longitude < max_longitude)"
        ");"

        "CREATE TABLE parking_sessions ("
        "    id INTEGER PRIMARY KEY,"
        "    vehicle_number TEXT NOT NULL,"
        "    latitude REAL NOT NULL,"
        "    longitude REAL NOT NULL,"
        "    city_id INTEGER NOT NULL,"
        "    price_per_minute REAL NOT NULL,"
        "    start_time INTEGER NOT NULL,"
        "    end_time INTEGER,"
        "    cost REAL"
        ");"

        "INSERT INTO parking_cities "
        "(city, min_latitude, max_latitude, "
        "min_longitude, max_longitude, price_per_minute) "
        "VALUES "
        "('Tel Aviv', 10.0, 19.9, 10.0, 19.9, 0.50),"
        "('Haifa', 20.0, 29.9, 20.0, 29.9, 0.40),"
        "('Jerusalem', 30.0, 39.9, 30.0, 39.9, 0.60);"

        "COMMIT;";

    if (database == NULL)
    {
        LOG_ERROR("SQLite database object is NULL.");
        return -1;
    }

    if (database->database == NULL)
    {
        LOG_ERROR("SQLite database is not open.");
        return -1;
    }

    result = sqlite3_exec(
        database->database,
        initialize_sql,
        NULL,
        NULL,
        &error_message);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to initialize SQLite database: %s.",
            error_message != NULL
                ? error_message
                : sqlite3_errmsg(database->database));

        if (error_message != NULL)
        {
            sqlite3_free(error_message);
        }

        sqlite3_exec(
            database->database,
            "ROLLBACK;",
            NULL,
            NULL,
            NULL);

        return -1;
    }

    LOG_INFO("SQLite database schema initialized successfully.");

    return 0;
}

/**
 * @brief Adds a new parking city to the database.
 *
 * Inserts a new city, its rectangular coordinate boundaries,
 * and the parking price per minute into the parking_cities table.
 *
 * @param database Pointer to the SQLite database object.
 * @param city City name.
 * @param min_latitude Minimum latitude coordinate.
 * @param max_latitude Maximum latitude coordinate.
 * @param min_longitude Minimum longitude coordinate.
 * @param max_longitude Maximum longitude coordinate.
 * @param price_per_minute Parking price per minute.
 *
 * @return
 *      - 0  Success.
 *      - -1 Operation failed.
 */
int sqlite_database_add_city(
    sqlite_database_t* database,
    const char* city,
    double min_latitude,
    double max_latitude,
    double min_longitude,
    double max_longitude,
    double price_per_minute)
{
    const char* sql =
        "INSERT INTO parking_cities "
        "(city, min_latitude, max_latitude, "
        "min_longitude, max_longitude, price_per_minute) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* statement = NULL;
    int result;

    if ((database == NULL) ||
        (database->database == NULL) ||
        (city == NULL))
    {
        LOG_ERROR("Invalid argument passed to sqlite_database_add_city().");
        return -1;
    }

    if ((min_latitude >= max_latitude) ||
        (min_longitude >= max_longitude) ||
        (price_per_minute < 0.0))
    {
        LOG_ERROR("Invalid city coordinates or parking price.");
        return -1;
    }

    result = sqlite3_prepare_v2(
        database->database,
        sql,
        -1,
        &statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare add city statement: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    sqlite3_bind_text(statement, 1, city, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 2, min_latitude);
    sqlite3_bind_double(statement, 3, max_latitude);
    sqlite3_bind_double(statement, 4, min_longitude);
    sqlite3_bind_double(statement, 5, max_longitude);
    sqlite3_bind_double(statement, 6, price_per_minute);

    result = sqlite3_step(statement);

    if (result != SQLITE_DONE)
    {
        LOG_ERROR(
            "Failed to add city '%s': %s.",
            city,
            sqlite3_errmsg(database->database));

        sqlite3_finalize(statement);
        return -1;
    }

    sqlite3_finalize(statement);

    LOG_INFO("City '%s' added successfully.", city);

    return 0;
}

/**
 * @brief Updates the parking price for an existing city.
 *
 * Finds the city by name and replaces its current parking price
 * with the specified value.
 *
 * @param database Pointer to the SQLite database object.
 * @param city City name.
 * @param price_per_minute New parking price per minute.
 *
 * @return
 *      - 0  Success.
 *      - -1 Operation failed or the city was not found.
 */
int sqlite_database_update_city_price(
    sqlite_database_t* database,
    const char* city,
    double price_per_minute)
{
    const char* sql =
        "UPDATE parking_cities "
        "SET price_per_minute = ? "
        "WHERE city = ?;";

    sqlite3_stmt* statement = NULL;
    int result;

    if ((database == NULL) ||
        (database->database == NULL) ||
        (city == NULL))
    {
        LOG_ERROR(
            "Invalid argument passed to "
            "sqlite_database_update_city_price().");

        return -1;
    }

    if (price_per_minute < 0.0)
    {
        LOG_ERROR("Parking price cannot be negative.");
        return -1;
    }

    result = sqlite3_prepare_v2(
        database->database,
        sql,
        -1,
        &statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare update city price statement: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    sqlite3_bind_double(statement, 1, price_per_minute);
    sqlite3_bind_text(statement, 2, city, -1, SQLITE_TRANSIENT);

    result = sqlite3_step(statement);

    if (result != SQLITE_DONE)
    {
        LOG_ERROR(
            "Failed to update price for city '%s': %s.",
            city,
            sqlite3_errmsg(database->database));

        sqlite3_finalize(statement);
        return -1;
    }

    if (sqlite3_changes(database->database) == 0)
    {
        LOG_ERROR("City '%s' was not found.", city);

        sqlite3_finalize(statement);
        return -1;
    }

    sqlite3_finalize(statement);

    LOG_INFO(
        "Parking price for city '%s' updated to %.2f.",
        city,
        price_per_minute);

    return 0;
}

/**
 * @brief Deletes a city from the database.
 *
 * Removes the city identified by its name from the parking_cities table.
 *
 * @param database Pointer to the SQLite database object.
 * @param city City name.
 *
 * @return
 *      - 0  Success.
 *      - -1 Operation failed or the city was not found.
 */
int sqlite_database_delete_city(
    sqlite_database_t* database,
    const char* city)
{
    const char* sql =
        "DELETE FROM parking_cities "
        "WHERE city = ?;";

    sqlite3_stmt* statement = NULL;
    int result;

    if ((database == NULL) ||
        (database->database == NULL) ||
        (city == NULL))
    {
        LOG_ERROR(
            "Invalid argument passed to "
            "sqlite_database_delete_city().");

        return -1;
    }

    result = sqlite3_prepare_v2(
        database->database,
        sql,
        -1,
        &statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare delete city statement: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    sqlite3_bind_text(statement, 1, city, -1, SQLITE_TRANSIENT);

    result = sqlite3_step(statement);

    if (result != SQLITE_DONE)
    {
        LOG_ERROR(
            "Failed to delete city '%s': %s.",
            city,
            sqlite3_errmsg(database->database));

        sqlite3_finalize(statement);
        return -1;
    }

    if (sqlite3_changes(database->database) == 0)
    {
        LOG_ERROR("City '%s' was not found.", city);

        sqlite3_finalize(statement);
        return -1;
    }

    sqlite3_finalize(statement);

    LOG_INFO("City '%s' deleted successfully.", city);

    return 0;
}

/**
 * @brief Finds a parking city by coordinates.
 *
 * Searches for a city whose rectangular parking area contains
 * the specified latitude and longitude.
 *
 * If a matching city is found, the function returns its database
 * identifier through city_id.
 *
 * @param database Pointer to the SQLite database object.
 * @param latitude Current latitude coordinate.
 * @param longitude Current longitude coordinate.
 * @param city_id Pointer that receives the city identifier.
 *
 * @return
 *      - 0  City found.
 *      - -1 Operation failed or no matching city was found.
 */
int sqlite_database_find_city(
    sqlite_database_t* database,
    double latitude,
    double longitude,
    int* city_id)
{
    const char* sql =
        "SELECT id "
        "FROM parking_cities "
        "WHERE ? >= min_latitude "
        "AND ? <= max_latitude "
        "AND ? >= min_longitude "
        "AND ? <= max_longitude "
        "LIMIT 1;";

    sqlite3_stmt* statement = NULL;
    int result;

    if ((database == NULL) ||
        (database->database == NULL) ||
        (city_id == NULL))
    {
        LOG_ERROR(
            "Invalid argument passed to "
            "sqlite_database_find_city().");

        return -1;
    }

    result = sqlite3_prepare_v2(
        database->database,
        sql,
        -1,
        &statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare find city statement: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    sqlite3_bind_double(statement, 1, latitude);
    sqlite3_bind_double(statement, 2, latitude);
    sqlite3_bind_double(statement, 3, longitude);
    sqlite3_bind_double(statement, 4, longitude);

    result = sqlite3_step(statement);

    if (result == SQLITE_ROW)
    {
        *city_id = sqlite3_column_int(statement, 0);

        sqlite3_finalize(statement);

        LOG_DEBUG(
            "Coordinates %.2f, %.2f mapped to city ID %d.",
            latitude,
            longitude,
            *city_id);

        return 0;
    }

    if (result == SQLITE_DONE)
    {
        LOG_ERROR(
            "No city found for coordinates %.2f, %.2f.",
            latitude,
            longitude);
    }
    else
    {
        LOG_ERROR(
            "Failed to find city by coordinates: %s.",
            sqlite3_errmsg(database->database));
    }

    sqlite3_finalize(statement);

    return -1;
}

/**
 * @brief Retrieves complete city information by identifier.
 *
 * Reads the city name, rectangular coordinate boundaries,
 * and parking price from the parking_cities table.
 *
 * @param database Pointer to the SQLite database object.
 * @param city_id City identifier.
 * @param city Buffer that receives the city name.
 * @param city_size Size of the city name buffer.
 * @param min_latitude Pointer that receives the minimum latitude.
 * @param max_latitude Pointer that receives the maximum latitude.
 * @param min_longitude Pointer that receives the minimum longitude.
 * @param max_longitude Pointer that receives the maximum longitude.
 * @param price_per_minute Pointer that receives the parking price.
 *
 * @return
 *      - 0  Success.
 *      - -1 Operation failed or the city was not found.
 */
int sqlite_database_get_city(
    sqlite_database_t* database,
    int city_id,
    char* city,
    size_t city_size,
    double* min_latitude,
    double* max_latitude,
    double* min_longitude,
    double* max_longitude,
    double* price_per_minute)
{
    const char* sql =
        "SELECT city, "
        "min_latitude, "
        "max_latitude, "
        "min_longitude, "
        "max_longitude, "
        "price_per_minute "
        "FROM parking_cities "
        "WHERE id = ?;";

    sqlite3_stmt* statement = NULL;
    const unsigned char* city_text;
    int result;

    if ((database == NULL) ||
        (database->database == NULL) ||
        (city_id <= 0) ||
        (city == NULL) ||
        (city_size == 0) ||
        (min_latitude == NULL) ||
        (max_latitude == NULL) ||
        (min_longitude == NULL) ||
        (max_longitude == NULL) ||
        (price_per_minute == NULL))
    {
        LOG_ERROR(
            "Invalid argument passed to "
            "sqlite_database_get_city().");

        return -1;
    }

    result = sqlite3_prepare_v2(
        database->database,
        sql,
        -1,
        &statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare get city statement: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    sqlite3_bind_int(statement, 1, city_id);

    result = sqlite3_step(statement);

    if (result == SQLITE_ROW)
    {
        city_text = sqlite3_column_text(statement, 0);

        if (city_text == NULL)
        {
            LOG_ERROR("Database returned an invalid city name.");

            sqlite3_finalize(statement);
            return -1;
        }

        snprintf(
            city,
            city_size,
            "%s",
            (const char*)city_text);

        *min_latitude =
            sqlite3_column_double(statement, 1);

        *max_latitude =
            sqlite3_column_double(statement, 2);

        *min_longitude =
            sqlite3_column_double(statement, 3);

        *max_longitude =
            sqlite3_column_double(statement, 4);

        *price_per_minute =
            sqlite3_column_double(statement, 5);

        sqlite3_finalize(statement);

        LOG_DEBUG(
            "City ID %d retrieved successfully.",
            city_id);

        return 0;
    }

    if (result == SQLITE_DONE)
    {
        LOG_ERROR(
            "City with ID %d was not found.",
            city_id);
    }
    else
    {
        LOG_ERROR(
            "Failed to retrieve city with ID %d: %s.",
            city_id,
            sqlite3_errmsg(database->database));
    }

    sqlite3_finalize(statement);

    return -1;
}

/**
 * @brief Starts a new parking session.
 *
 * Verifies that the vehicle does not already have an active parking
 * session, retrieves the current parking price for the specified city,
 * and creates a new parking session.
 *
 * The parking price is stored together with the session to preserve the
 * tariff that was active when the parking session started.
 *
 * @param database Pointer to the SQLite database object.
 * @param vehicle_number Vehicle registration number.
 * @param latitude Current latitude coordinate.
 * @param longitude Current longitude coordinate.
 * @param city_id City identifier.
 * @param start_time Parking start time as a Unix timestamp.
 *
 * @return
 *      - 0  Success.
 *      - -1 Operation failed or the vehicle is already parked.
 */
int sqlite_database_start_parking(
    sqlite_database_t* database,
    const char* vehicle_number,
    double latitude,
    double longitude,
    int city_id,
    int64_t start_time)
{
    const char* active_session_sql =
        "SELECT id "
        "FROM parking_sessions "
        "WHERE vehicle_number = ? "
        "AND end_time IS NULL "
        "LIMIT 1;";

    const char* select_price_sql =
        "SELECT price_per_minute "
        "FROM parking_cities "
        "WHERE id = ?;";

    const char* insert_sql =
        "INSERT INTO parking_sessions "
        "(vehicle_number, latitude, longitude, city_id, "
        "price_per_minute, start_time) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* active_session_statement = NULL;
    sqlite3_stmt* price_statement = NULL;
    sqlite3_stmt* insert_statement = NULL;

    double price_per_minute;
    int result;

    if ((database == NULL) ||
        (database->database == NULL) ||
        (vehicle_number == NULL) ||
        (city_id <= 0) ||
        (start_time <= 0))
    {
        LOG_ERROR(
            "Invalid argument passed to "
            "sqlite_database_start_parking().");

        return -1;
    }

    /*
     * Check whether the vehicle already has an active
     * parking session.
     */
    result = sqlite3_prepare_v2(
        database->database,
        active_session_sql,
        -1,
        &active_session_statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare active parking check: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    result = sqlite3_bind_text(
        active_session_statement,
        1,
        vehicle_number,
        -1,
        SQLITE_TRANSIENT);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to bind vehicle number: %s.",
            sqlite3_errmsg(database->database));

        sqlite3_finalize(active_session_statement);
        return -1;
    }

    result = sqlite3_step(active_session_statement);

    if (result == SQLITE_ROW)
    {
        LOG_ERROR(
            "Vehicle '%s' already has an active parking session.",
            vehicle_number);

        sqlite3_finalize(active_session_statement);
        return -1;
    }

    if (result != SQLITE_DONE)
    {
        LOG_ERROR(
            "Failed to check active parking session: %s.",
            sqlite3_errmsg(database->database));

        sqlite3_finalize(active_session_statement);
        return -1;
    }

    sqlite3_finalize(active_session_statement);

    /*
     * Retrieve the current parking price for the specified city.
     */
    result = sqlite3_prepare_v2(
        database->database,
        select_price_sql,
        -1,
        &price_statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare city price lookup: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    result = sqlite3_bind_int(
        price_statement,
        1,
        city_id);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to bind city ID: %s.",
            sqlite3_errmsg(database->database));

        sqlite3_finalize(price_statement);
        return -1;
    }

    result = sqlite3_step(price_statement);

    if (result == SQLITE_DONE)
    {
        LOG_ERROR(
            "City with ID %d was not found.",
            city_id);

        sqlite3_finalize(price_statement);
        return -1;
    }

    if (result != SQLITE_ROW)
    {
        LOG_ERROR(
            "Failed to retrieve parking price: %s.",
            sqlite3_errmsg(database->database));

        sqlite3_finalize(price_statement);
        return -1;
    }

    price_per_minute =
        sqlite3_column_double(price_statement, 0);

    sqlite3_finalize(price_statement);

    /*
     * Create a new parking session using the price that
     * is active at the moment the parking starts.
     */
    result = sqlite3_prepare_v2(
        database->database,
        insert_sql,
        -1,
        &insert_statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare start parking statement: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    result = sqlite3_bind_text(
        insert_statement,
        1,
        vehicle_number,
        -1,
        SQLITE_TRANSIENT);

    if (result == SQLITE_OK)
    {
        result = sqlite3_bind_double(
            insert_statement,
            2,
            latitude);
    }

    if (result == SQLITE_OK)
    {
        result = sqlite3_bind_double(
            insert_statement,
            3,
            longitude);
    }

    if (result == SQLITE_OK)
    {
        result = sqlite3_bind_int(
            insert_statement,
            4,
            city_id);
    }

    if (result == SQLITE_OK)
    {
        result = sqlite3_bind_double(
            insert_statement,
            5,
            price_per_minute);
    }

    if (result == SQLITE_OK)
    {
        result = sqlite3_bind_int64(
            insert_statement,
            6,
            start_time);
    }

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to bind start parking parameters: %s.",
            sqlite3_errmsg(database->database));

        sqlite3_finalize(insert_statement);
        return -1;
    }

    result = sqlite3_step(insert_statement);

    if (result != SQLITE_DONE)
    {
        LOG_ERROR(
            "Failed to start parking for vehicle '%s': %s.",
            vehicle_number,
            sqlite3_errmsg(database->database));

        sqlite3_finalize(insert_statement);
        return -1;
    }

    sqlite3_finalize(insert_statement);

    LOG_INFO(
        "Parking started successfully for vehicle '%s' "
        "with price %.2f per minute.",
        vehicle_number,
        price_per_minute);

    return 0;
}

/**
 * @brief Stops an active parking session.
 *
 * Finds the active parking session for the specified vehicle,
 * reads the stored parking start time and price, calculates the
 * parking cost, and updates the session with its end time and cost.
 *
 * Every started parking minute is charged as a complete minute.
 *
 * @param database Pointer to the SQLite database object.
 * @param vehicle_number Vehicle registration number.
 * @param end_time Parking end time as a Unix timestamp.
 * @param start_time Pointer that receives the stored parking start time.
 * @param cost Pointer that receives the calculated parking cost.
 *
 * @return
 *      - 0  Success.
 *      - -1 Operation failed or no active session was found.
 */
int sqlite_database_stop_parking(
    sqlite_database_t* database,
    const char* vehicle_number,
    int64_t end_time,
    int64_t* start_time,
    double* cost)
{
    const char* select_sql =
        "SELECT id, start_time, price_per_minute "
        "FROM parking_sessions "
        "WHERE vehicle_number = ? "
        "AND end_time IS NULL "
        "ORDER BY id DESC "
        "LIMIT 1;";

    const char* update_sql =
        "UPDATE parking_sessions "
        "SET end_time = ?, cost = ? "
        "WHERE id = ?;";

    sqlite3_stmt* select_statement = NULL;
    sqlite3_stmt* update_statement = NULL;

    int64_t session_id;
    int64_t stored_start_time;
    int64_t duration_seconds;
    int64_t duration_minutes;

    double price_per_minute;
    double calculated_cost;

    int result;

    if ((database == NULL) ||
        (database->database == NULL) ||
        (vehicle_number == NULL) ||
        (end_time <= 0) ||
        (start_time == NULL) ||
        (cost == NULL))
    {
        LOG_ERROR(
            "Invalid argument passed to "
            "sqlite_database_stop_parking().");

        return -1;
    }

    /*
     * Find the active parking session and retrieve
     * the stored start time and parking price.
     */
    result = sqlite3_prepare_v2(
        database->database,
        select_sql,
        -1,
        &select_statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare active parking search: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    result = sqlite3_bind_text(
        select_statement,
        1,
        vehicle_number,
        -1,
        SQLITE_TRANSIENT);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to bind vehicle number: %s.",
            sqlite3_errmsg(database->database));

        sqlite3_finalize(select_statement);
        return -1;
    }

    result = sqlite3_step(select_statement);

    if (result == SQLITE_DONE)
    {
        LOG_ERROR(
            "Active parking session for vehicle '%s' was not found.",
            vehicle_number);

        sqlite3_finalize(select_statement);
        return -1;
    }

    if (result != SQLITE_ROW)
    {
        LOG_ERROR(
            "Failed to read active parking session: %s.",
            sqlite3_errmsg(database->database));

        sqlite3_finalize(select_statement);
        return -1;
    }

    session_id =
        sqlite3_column_int64(select_statement, 0);

    stored_start_time =
        sqlite3_column_int64(select_statement, 1);

    price_per_minute =
        sqlite3_column_double(select_statement, 2);

    sqlite3_finalize(select_statement);

    if (end_time < stored_start_time)
    {
        LOG_ERROR(
            "Parking end time is earlier than start time "
            "for vehicle '%s'.",
            vehicle_number);

        return -1;
    }

    /*
     * Calculate the number of started parking minutes.
     */
    duration_seconds =
        end_time - stored_start_time;

    duration_minutes =
        (duration_seconds + 59) / 60;

    calculated_cost =
        (double)duration_minutes *
        price_per_minute;

    /*
     * Store the parking end time and calculated cost.
     */
    result = sqlite3_prepare_v2(
        database->database,
        update_sql,
        -1,
        &update_statement,
        NULL);

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to prepare stop parking statement: %s.",
            sqlite3_errmsg(database->database));

        return -1;
    }

    result = sqlite3_bind_int64(
        update_statement,
        1,
        end_time);

    if (result == SQLITE_OK)
    {
        result = sqlite3_bind_double(
            update_statement,
            2,
            calculated_cost);
    }

    if (result == SQLITE_OK)
    {
        result = sqlite3_bind_int64(
            update_statement,
            3,
            session_id);
    }

    if (result != SQLITE_OK)
    {
        LOG_ERROR(
            "Failed to bind stop parking parameters: %s.",
            sqlite3_errmsg(database->database));

        sqlite3_finalize(update_statement);
        return -1;
    }

    result = sqlite3_step(update_statement);

    if (result != SQLITE_DONE)
    {
        LOG_ERROR(
            "Failed to stop parking for vehicle '%s': %s.",
            vehicle_number,
            sqlite3_errmsg(database->database));

        sqlite3_finalize(update_statement);
        return -1;
    }

    sqlite3_finalize(update_statement);

    *start_time = stored_start_time;
    *cost = calculated_cost;

    LOG_INFO(
        "Parking stopped successfully for vehicle '%s'. "
        "Duration: %lld minute(s), cost: %.2f.",
        vehicle_number,
        (long long)duration_minutes,
        calculated_cost);

    return 0;
}
