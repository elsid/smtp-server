#ifndef SMTP_SERVER_SETTINGS_H
#define SMTP_SERVER_SETTINGS_H

#include <libconfig.h>
#include <stdint.h>

typedef struct settings {
    config_t config;
    const char *address;
    uint16_t port;
    int workers_count;
    int backlog_size;
    const char *maildir;
    const char *log;
    int max_in_message_size;
    long long timeout;
    int daemon;
} settings_t;

int settings_init(settings_t *settings, const char *file_name);
void settings_destroy(settings_t *settings);

#endif
