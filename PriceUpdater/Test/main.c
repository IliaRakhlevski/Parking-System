#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "logger.h"
#include "sqlite_database.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>


/**
 * @brief Path to the Parking System configuration file.
 *
 * The path is relative to the PriceUpdater working directory.
 */
#define CONFIG_FILE_PATH "../Config/parking_system.conf"

/**
 * @brief Configuration key containing the PriceUpdater log file path.
 */
#define PRICE_UPDATER_LOG_FILE_KEY "PRICE_UPDATER_LOG_FILE"

/**
 * @brief Configuration key controlling console log output.
 */
#define ENABLE_CONSOLE_LOGGING_KEY "ENABLE_CONSOLE_LOGGING"

/**
 * @brief Maximum length of a city name.
 *
 * Includes the terminating null character.
 */
#define CITY_NAME_LENGTH    64

/**
 * @brief Maximum length of a configuration value.
 *
 * Used for storing file paths and other string values
 * read from the configuration file.
 *
 * The value includes the terminating null character.
 */
#define CONFIG_MAX_VALUE_LENGTH 256


static void show_menu(void);
static void clear_input_buffer(void);
static int add_city(sqlite_database_t *database);
static int initialize(config_t *config, sqlite_database_t *database, char *pid_file_path, size_t pid_file_path_size);
static int update_city_price(sqlite_database_t *database);
static int delete_city(sqlite_database_t *database);
static int read_database_pid(const char *pid_file_path, pid_t *database_pid);
static int notify_database(const char *pid_file_path);


int main(void)
{
    config_t config = {0};

    sqlite_database_t database = {0};

    char database_pid_file_path[CONFIG_MAX_VALUE_LENGTH];

    int menu_item;

    bool running = true;

    if (initialize(
        &config,
        &database,
        database_pid_file_path,
        sizeof(database_pid_file_path)) != 0)
    {
        return -1;
    }
  
    LOG_INFO("PriceUpdater started successfully.");

    LOG_INFO("Configuration file loaded: %s.", CONFIG_FILE_PATH);

    while (running)
    {
        show_menu();

        if (scanf("%d", &menu_item) != 1)
        {
            printf("Invalid input.\n");

            clear_input_buffer();

            continue;
        }

        clear_input_buffer();

        switch (menu_item)
        {
            case 1:
                if (add_city(&database) == 0)
                {
                    notify_database(database_pid_file_path);
                }
                break;

            case 2:
                if (update_city_price(&database) == 0)
                {
                    notify_database(database_pid_file_path);
                }
                break;

            case 3:
                if (delete_city(&database) == 0)
                {
                     notify_database(database_pid_file_path);
                }
                break;

            case 4:
                running = false;
                break;

            default:
                printf("Invalid menu item.\n");
                break;
        }
    }

    sqlite_database_close(&database);

    logger_close();

    LOG_INFO("PriceUpdater stopped successfully.");

    return 0;
}

/**
 * @brief Reads city data and adds a new city to the database.
 *
 * Reads the city name, coordinate boundaries and parking price
 * from standard input, then stores the city in the SQLite database.
 *
 * @param database Pointer to an opened SQLite database object.
 * @return
 *      - 0  Success.
 *      - -1 Failed.
 */
static int add_city(sqlite_database_t *database)
{
    char city[CITY_NAME_LENGTH];

    double min_latitude;
    double max_latitude;
    double min_longitude;
    double max_longitude;
    double price_per_minute;

    if (database == NULL)
    {
        LOG_ERROR("SQLite database object is NULL.");
        return -1;
    }

    LOG_INFO("Add city operation started.");

    printf("\n=== Add City ===\n");

    printf("City name: ");

    if (fgets(city, sizeof(city), stdin) == NULL)
    {
        LOG_ERROR("Failed to read city name.");
        return -1;
    }

    city[strcspn(city, "\n")] = '\0';

    if (city[0] == '\0')
    {
        LOG_ERROR("City name cannot be empty.");
        return -1;
    }

    printf("Minimum latitude: ");

    if (scanf("%lf", &min_latitude) != 1)
    {
        LOG_ERROR("Invalid minimum latitude.");
        clear_input_buffer();
        return -1;
    }

    clear_input_buffer();

    printf("Maximum latitude: ");

    if (scanf("%lf", &max_latitude) != 1)
    {
        LOG_ERROR("Invalid maximum latitude.");
        clear_input_buffer();
        return -1;
    }

    clear_input_buffer();

    printf("Minimum longitude: ");

    if (scanf("%lf", &min_longitude) != 1)
    {
        LOG_ERROR("Invalid minimum longitude.");
        clear_input_buffer();
        return -1;
    }

    clear_input_buffer();

    printf("Maximum longitude: ");

    if (scanf("%lf", &max_longitude) != 1)
    {
        LOG_ERROR("Invalid maximum longitude.");
        clear_input_buffer();
        return -1;
    }

    clear_input_buffer();

    printf("Price per minute: ");

    if (scanf("%lf", &price_per_minute) != 1)
    {
        LOG_ERROR("Invalid parking price.");
        clear_input_buffer();
        return -1;
    }

    clear_input_buffer();

    if (sqlite_database_add_city(
            database,
            city,
            min_latitude,
            max_latitude,
            min_longitude,
            max_longitude,
            price_per_minute) != 0)
    {
        LOG_ERROR(
            "Failed to add city '%s'.",
            city);

        return -1;
    }

    return 0;
}

/**
 * @brief Updates the parking price for an existing city.
 *
 * Reads the city name and new parking price from standard input,
 * then updates the corresponding database record.
 *
 * @param database Pointer to an opened SQLite database object.
 * @return
 *      - 0  Success.
 *      - -1 Failed.
 */
static int update_city_price(sqlite_database_t *database)
{
    char city[CITY_NAME_LENGTH];
    double price_per_minute;

    if (database == NULL)
    {
        LOG_ERROR("SQLite database object is NULL.");
        return -1;
    }

    LOG_INFO("Update city price operation started.");

    printf("\n=== Update City Price ===\n");

    printf("City name: ");

    if (fgets(city, sizeof(city), stdin) == NULL)
    {
        LOG_ERROR("Failed to read city name.");
        return -1;
    }

    city[strcspn(city, "\n")] = '\0';

    if (city[0] == '\0')
    {
        LOG_ERROR("City name cannot be empty.");
        return -1;
    }

    printf("New price per minute: ");

    if (scanf("%lf", &price_per_minute) != 1)
    {
        LOG_ERROR("Invalid parking price.");
        clear_input_buffer();
        return -1;
    }

    clear_input_buffer();

    if (sqlite_database_update_city_price(
            database,
            city,
            price_per_minute) != 0)
    {
        LOG_ERROR(
            "Failed to update parking price for city '%s'.",
            city);

        return -1;
    }

    return 0;
}

/**
 * @brief Deletes an existing parking city.
 *
 * Reads the city name from standard input and removes the corresponding
 * record from the SQLite database.
 *
 * @param database Pointer to an opened SQLite database object.
 * @return
 *      - 0  Success.
 *      - -1 Failed.
 */
static int delete_city(sqlite_database_t *database)
{
    char city[CITY_NAME_LENGTH];

    if (database == NULL)
    {
        LOG_ERROR("SQLite database object is NULL.");
        return -1;
    }

    LOG_INFO("Delete city operation started.");

    printf("\n=== Delete City ===\n");

    printf("City name: ");

    if (fgets(city, sizeof(city), stdin) == NULL)
    {
        LOG_ERROR("Failed to read city name.");
        return -1;
    }

    city[strcspn(city, "\n")] = '\0';

    if (city[0] == '\0')
    {
        LOG_ERROR("City name cannot be empty.");
        return -1;
    }

    if (sqlite_database_delete_city(
            database,
            city) != 0)
    {
        LOG_ERROR(
            "Failed to delete city '%s'.",
            city);

        return -1;
    }

    return 0;
}

/**
 * @brief Clears the standard input buffer.
 *
 * Removes all remaining characters from stdin
 * until a newline character or EOF is reached.
 */
static void clear_input_buffer(void)
{
    int character;

    while (((character = getchar()) != '\n') &&
           (character != EOF))
    {
    }
}

/**
 * @brief Displays the main menu.
 */
static void show_menu(void)
{
    printf("\n");
    printf("=====================================\n");
    printf("       Parking Price Updater\n");
    printf("=====================================\n");
    printf("1. Add city\n");
    printf("2. Update city price\n");
    printf("3. Delete city\n");
    printf("4. Exit\n");
    printf("\n");
    printf("Select: ");
}

/**
 * @brief Initializes the PriceUpdater module.
 *
 * Loads the configuration file, initializes the logger,
 * reads the SQLite database path and opens the database.
 *
 * @param config Pointer to the configuration object.
 * @param database Pointer to the SQLite database object.
 *
 * @return
 *      - 0  Success.
 *      - -1 Initialization failed.
 */
static int initialize(config_t *config, sqlite_database_t *database, char *pid_file_path, size_t pid_file_path_size)
{
    const char *log_file_path;

    const char *database_file_path;

    const char *configured_pid_file_path;

    int console_logging_enabled;

    /*
     * Load the common Parking System configuration file.
     *
     * The logger is not initialized yet, therefore errors at this stage
     * are written directly to stderr.
     */
    if (config_load(config, CONFIG_FILE_PATH) != 0)
    {
        fprintf(
            stderr,
            "Failed to load configuration file: %s\n",
            CONFIG_FILE_PATH);

        return 1;
    }

    log_file_path = config_get_string(
        config,
        PRICE_UPDATER_LOG_FILE_KEY,
        NULL);

    if (log_file_path == NULL)
    {
        fprintf(
            stderr,
            "Configuration key '%s' was not found.\n",
            PRICE_UPDATER_LOG_FILE_KEY);

        return 1;
    }

    console_logging_enabled = config_get_int(
        config,
        ENABLE_CONSOLE_LOGGING_KEY,
        1);

    configured_pid_file_path = config_get_string(
        config,
        "DATABASE_PID_FILE",
        NULL);

    if (configured_pid_file_path == NULL)
    {
        LOG_ERROR(
            "Configuration key 'DATABASE_PID_FILE' was not found.");

        return -1;
    }

    if (snprintf(
            pid_file_path,
            pid_file_path_size,
            "%s",
            configured_pid_file_path) >= (int)pid_file_path_size)
    {
        LOG_ERROR("Database PID file path is too long.");

        return -1;
    }

    if (logger_init(
            log_file_path,
            console_logging_enabled) != 0)
    {
        fprintf(
            stderr,
            "Failed to initialize logger: %s\n",
            log_file_path);

        return 1;
    }

    database_file_path = config_get_string(
        config,
        "DATABASE_PATH",
        NULL);

    if (database_file_path == NULL)
    {
        LOG_ERROR("Configuration key 'DATABASE_PATH' was not found.");

        logger_close();
        return 1;
    }

    if (sqlite_database_open(
            database,
            database_file_path) != 0)
    {
        LOG_ERROR(
            "Failed to open SQLite database '%s'.",
            database_file_path);

        logger_close();
        return 1;
    }

    return 0;
}

#include <sys/types.h>

/**
 * @brief Reads the Database process identifier from the PID file.
 *
 * @param pid_file_path Path to the Database PID file.
 * @param database_pid Pointer to the variable receiving the PID.
 *
 * @return
 *      - 0  Success.
 *      - -1 Failed to read the PID.
 */
static int read_database_pid(const char *pid_file_path, pid_t *database_pid)
{
    FILE *file;

    if ((pid_file_path == NULL) ||
        (database_pid == NULL))
    {
        LOG_ERROR("Invalid function arguments.");

        return -1;
    }

    file = fopen(pid_file_path, "r");

    if (file == NULL)
    {
        LOG_ERROR(
            "Failed to open PID file '%s'.",
            pid_file_path);

        return -1;
    }

    if (fscanf(file, "%d", database_pid) != 1)
    {
        LOG_ERROR(
            "Failed to read Database PID from '%s'.",
            pid_file_path);

        fclose(file);

        return -1;
    }

    fclose(file);

    LOG_INFO(
        "Database PID: %d.",
        *database_pid);

    return 0;
}

/**
 * @brief Sends a notification signal to the Database process.
 *
 * Reads the Database process identifier from the PID file and sends
 * SIGUSR1 to notify the Database that parking prices have changed.
 *
 * @param pid_file_path Path to the Database PID file.
 *
 * @return
 *      - 0  Notification sent successfully.
 *      - -1 Failed to notify the Database process.
 */
static int notify_database(const char *pid_file_path)
{
    pid_t database_pid;

    if (pid_file_path == NULL)
    {
        LOG_ERROR("Database PID file path is NULL.");

        return -1;
    }

    if (read_database_pid(
            pid_file_path,
            &database_pid) != 0)
    {
        return -1;
    }

    if (kill(database_pid, SIGUSR1) != 0)
    {
        LOG_ERROR(
            "Failed to send SIGUSR1 to Database process %d.",
            database_pid);

        return -1;
    }

    LOG_INFO(
        "SIGUSR1 sent to Database process %d.",
        database_pid);

    return 0;
}
