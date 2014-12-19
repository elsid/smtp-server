#ifndef SMTP_SERVER_CONTEXT_H
#define SMTP_SERVER_CONTEXT_H

#include <uuid/uuid.h>

#include "buffer.h"
#include "buffer_tailq.h"
#include "fsm.h"
#include "log.h"
#include "settings.h"
#include "transaction.h"

#define COMMAND_SIZE 5
#define UUID_STRING_SIZE (2 * sizeof(uuid_t) + 1)

typedef struct context {
    const settings_t *settings;
    log_t *log;
    te_smtp_server_state state;
    buffer_t in_message;
    buffer_tailq_t out_message_queue;
    int socket;
    int is_wait_transition;
    char command[COMMAND_SIZE];
    char uuid[UUID_STRING_SIZE];
    transaction_t transaction;
    struct timeval init_time;
    struct timeval last_action_time;
} context_t;

int context_init(context_t *context, const int sock, const settings_t *settings,
    log_t *log);
void context_destroy(context_t *context);

#endif
