#ifndef SMTP_SERVER_CONTEXT_H
#define SMTP_SERVER_CONTEXT_H

#include <uuid/uuid.h>

#include "buffer.h"
#include "buffer_tailq.h"
#include "fsm.h"
#include "log.h"
#include "settings.h"
#include "transaction.h"

typedef struct context {
    te_smtp_server_state state;
    buffer_t in_message;
    buffer_tailq_t out_message_queue;
    int sock;
    int is_wait_transition;
    char command[5];
    char uuid[2 * sizeof(uuid_t) + 1];
    transaction_t transaction;
    const settings_t *settings;
    log_t *log;
    struct timeval init_time;
    struct timeval last_action_time;
} context_t;

int context_init(context_t *context, const int sock, const settings_t *settings,
    log_t *log);
void context_destroy(context_t *context);

#endif
