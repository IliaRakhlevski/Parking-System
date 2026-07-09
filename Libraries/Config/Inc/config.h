#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of configuration entries.
 */
#define CONFIG_MAX_ENTRIES        100

/**
 * @brief Maximum length of a configuration key.
 */
#define CONFIG_MAX_KEY_LENGTH      64

/**
 * @brief Maximum length of a configuration value.
 */
#define CONFIG_MAX_VALUE_LENGTH   256

/**
 * @brief Single configuration key-value entry.
 */
typedef struct
{
    char key[CONFIG_MAX_KEY_LENGTH];
    char value[CONFIG_MAX_VALUE_LENGTH];
} config_entry_t;

/**
 * @brief Configuration storage object.
 */
typedef struct
{
    config_entry_t entries[CONFIG_MAX_ENTRIES];
    int count;
} config_t;

int config_load(config_t *config, const char *file_path);

const char *config_get_string(const config_t *config,
                              const char *key,
                              const char *default_value);

int config_get_int(const config_t *config,
                   const char *key,
                   int default_value);


#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */