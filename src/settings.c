#include "log.h"
#include "settings.h"

static int read_string(config_t *config, const char *path, const char **value)
{
    if (config_lookup_string(config, path, value) != CONFIG_TRUE) {
        PRINT_STDERR("error: no '%s' string setting in config", path);
        return -1;
    }

    return 0;
}

static int read_int(config_t *config, const char *path, int *value)
{
    if (config_lookup_int(config, path, value) != CONFIG_TRUE) {
        PRINT_STDERR("error: no '%s' int setting in config", path);
        return -1;
    }

    return 0;
}

static int read_int64(config_t *config, const char *path, long long *value)
{
    if (config_lookup_int64(config, path, value) != CONFIG_TRUE) {
        PRINT_STDERR("error: no '%s' int64 setting in config", path);
        return -1;
    }

    return 0;
}

static int read_uint16(config_t *config, const char *path, uint16_t *value)
{
    int int_value;

    if (read_int(config, path, &int_value) < 0) {
        return -1;
    }

    *value = int_value;

    return 0;
}

int settings_init(settings_t *settings, const char *file_name)
{
    config_t *config = &settings->config;
    config_init(config);

    if (config_read_file(config, file_name) != CONFIG_TRUE) {
        CALL_ERR_ARGS("config_read_file", "%s:%d %s", config_error_file(config),
            config_error_line(config), config_error_text(config));
        config_destroy(config);
        return -1;
    }

#define READ_STRING(name) if (read_string(config, #name, &settings->name) < 0) { return -1; }
#define READ_INT(name) if (read_int(config, #name, &settings->name) < 0) { return -1; }
#define READ_INT64(name) if (read_int64(config, #name, &settings->name) < 0) { return -1; }
#define READ_UINT16(name) if (read_uint16(config, #name, &settings->name) < 0) { return -1; }

    READ_STRING(address)
    READ_UINT16(port)
    READ_INT(workers_count)
    READ_INT(backlog_size)
    READ_STRING(maildir)
    READ_STRING(log)
    READ_INT(max_in_message_size)
    READ_INT64(timeout)
    READ_INT(daemon)

#undef READ_UINT16
#undef READ_INT64
#undef READ_INT
#undef READ_STRING

    if (settings->workers_count < 1) {
        PRINT_STDERR("error: workers_count < 1: %d", settings->workers_count);
        return -1;
    }

    return 0;
}

void settings_destroy(settings_t *settings)
{
    config_destroy(&settings->config);
}
