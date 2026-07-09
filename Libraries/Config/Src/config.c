#include "config.h"
#include "logger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Maximum length of one configuration file line.
 */
#define CONFIG_LINE_MAX_LENGTH 512

/**
 * @brief Remove leading and trailing spaces from a string.
 *
 * @param str String to trim.
 *
 * @return Pointer to the trimmed string.
 */
static char *config_trim(char *str)
{
    char *end;

    while ((*str == ' ') || (*str == '\t') || (*str == '\n') || (*str == '\r'))
    {
        str++;
    }

    if (*str == '\0')
    {
        return str;
    }

    end = str + strlen(str) - 1;

    while ((end > str) &&
           ((*end == ' ') || (*end == '\t') || (*end == '\n') || (*end == '\r')))
    {
        *end = '\0';
        end--;
    }

    return str;
}

/**
 * @brief Find configuration entry by key.
 *
 * @param config Configuration object.
 * @param key Configuration key.
 *
 * @return Pointer to entry if found, NULL otherwise.
 */
static const config_entry_t *config_find_entry(const config_t *config,
                                               const char *key)
{
    int i;

    if ((config == NULL) || (key == NULL))
    {
        return NULL;
    }

    for (i = 0; i < config->count; i++)
    {
        if (strcmp(config->entries[i].key, key) == 0)
        {
            return &config->entries[i];
        }
    }

    return NULL;
}

/**
 * @brief Load configuration file.
 *
 * @param config Configuration object.
 * @param file_path Configuration file path.
 *
 * @return
 *      - 0  Success.
 *      - -1 Failure.
 */
int config_load(config_t *config, const char *file_path)
{
    FILE *file;
    char line[CONFIG_LINE_MAX_LENGTH];
    int line_number = 0;

    if ((config == NULL) || (file_path == NULL))
    {
        LOG_ERROR("Invalid argument");
        return -1;
    }

    config->count = 0;

    file = fopen(file_path, "r");

    if (file == NULL)
    {
        LOG_ERROR("Failed to open config file: %s", file_path);
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *trimmed_line;
        char *separator;
        char *key;
        char *value;

        line_number++;

        trimmed_line = config_trim(line);

        if ((trimmed_line[0] == '\0') ||
            (trimmed_line[0] == '#') ||
            (trimmed_line[0] == ';'))
        {
            continue;
        }

        separator = strchr(trimmed_line, '=');

        if (separator == NULL)
        {
            LOG_ERROR("Invalid config line %d: %s", line_number, trimmed_line);
            continue;
        }

        *separator = '\0';

        key = config_trim(trimmed_line);
        value = config_trim(separator + 1);

        if ((key[0] == '\0') || (value[0] == '\0'))
        {
            LOG_ERROR("Invalid config line %d: empty key or value", line_number);
            continue;
        }

        if (config->count >= CONFIG_MAX_ENTRIES)
        {
            LOG_ERROR("Too many config entries. Maximum is %d", CONFIG_MAX_ENTRIES);
            fclose(file);
            return -1;
        }

        strncpy(config->entries[config->count].key,
                key,
                CONFIG_MAX_KEY_LENGTH - 1);
        config->entries[config->count].key[CONFIG_MAX_KEY_LENGTH - 1] = '\0';

        strncpy(config->entries[config->count].value,
                value,
                CONFIG_MAX_VALUE_LENGTH - 1);
        config->entries[config->count].value[CONFIG_MAX_VALUE_LENGTH - 1] = '\0';

        config->count++;
    }

    fclose(file);

    return 0;
}

/**
 * @brief Get string configuration value.
 *
 * @param config Configuration object.
 * @param key Configuration key.
 * @param default_value Default value returned if key is not found.
 *
 * @return Configuration value or default value.
 */
const char *config_get_string(const config_t *config,
                              const char *key,
                              const char *default_value)
{
    const config_entry_t *entry;

    entry = config_find_entry(config, key);

    if (entry == NULL)
    {
        LOG_ERROR("Configuration key not found: %s", key);
        return default_value;
    }

    return entry->value;
}

/**
 * @brief Get integer configuration value.
 *
 * @param config Configuration object.
 * @param key Configuration key.
 * @param default_value Default value returned if key is not found or conversion fails.
 *
 * @return Configuration integer value or default value.
 */
int config_get_int(const config_t *config,
                   const char *key,
                   int default_value)
{
    const config_entry_t *entry;
    char *endptr;
    long value;

    entry = config_find_entry(config, key);

    if (entry == NULL)
    {
        LOG_ERROR("Configuration key not found: %s", key);
        return default_value;
    }

    errno = 0;
    value = strtol(entry->value, &endptr, 10);

    if ((errno != 0) || (*endptr != '\0'))
    {
        LOG_ERROR("Invalid integer value for key %s: %s", key, entry->value);
        return default_value;
    }

    return (int)value;
}
