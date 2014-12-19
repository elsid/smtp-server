#include <assert.h>

#include "context.h"
#include "log.h"
#include "time.h"
#include "transaction.h"

#define INIT_BUFFER_SIZE 4096

int context_init(context_t *context, const int sock, const settings_t *settings,
    log_t *log)
{
    assert(NULL != context);

    if (buffer_init(&context->in_message, settings->max_in_message_size) < 0) {
        return -1;
    }

    TAILQ_INIT(&context->out_message_queue);

    if (transaction_init(&context->transaction, settings, log, sock) < 0) {
        buffer_destroy(&context->in_message);
        return -1;
    }

    if (gettimeofday(&context->init_time, NULL) < 0) {
        CALL_ERR("gettimeofday");
        return -1;
    }

    uuid_t uuid;

    uuid_generate(uuid);

    for (size_t i = 0; i < sizeof(uuid); ++i) {
        sprintf(context->uuid + 2 * i, "%02x", uuid[i]);
    }

    context->uuid[sizeof(context->uuid) - 1] = '\0';
    context->settings = settings;
    context->state = SMTP_SERVER_ST_INIT;
    context->socket = sock;
    context->is_wait_transition = 0;
    context->log = log;
    context->last_action_time = context->init_time;

    return 0;
}

void context_destroy(context_t *context)
{
    assert(NULL != context);

    struct timeval current_time;

    if (gettimeofday(&current_time, NULL) < 0) {
        CALL_ERR("gettimeofday");
    } else {
        struct timeval duration;

        timeval_diff(&duration, &context->init_time, &current_time);

        log_write(context->log, "[%s] session duration: %ld msec", context->uuid,
            timeval_to_msec(&duration));
    }

    transaction_destroy(&context->transaction);

    buffer_tailq_entry_t *item, *temp;

    TAILQ_FOREACH_SAFE(item, &context->out_message_queue, entry, temp) {
        TAILQ_REMOVE(&context->out_message_queue, item, entry);
        buffer_destroy(&item->buffer);
        free(item);
    }

    buffer_destroy(&context->in_message);
}
