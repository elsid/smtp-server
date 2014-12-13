#ifndef SMTP_SERVER_BUFFER_TAILQ_H
#define SMTP_SERVER_BUFFER_TAILQ_H

#include <bsd/sys/queue.h>

#include "buffer.h"

typedef TAILQ_HEAD(buffer_tailq, buffer_tailq_entry) buffer_tailq_t;

typedef struct buffer_tailq_entry {
   buffer_t buffer;
   TAILQ_ENTRY(buffer_tailq_entry) entry;
} buffer_tailq_entry_t;

void buffer_tailq_init(buffer_tailq_t *tailq);
void buffer_tailq_destroy(buffer_tailq_t *tailq);
int buffer_tailq_push_back(buffer_tailq_t *tailq, const void *data,
    const size_t size);
int buffer_tailq_push_back_string(buffer_tailq_t *tailq, const char *string);
buffer_t *buffer_tailq_front(buffer_tailq_t *tailq);
void buffer_tailq_pop_front(buffer_tailq_t *tailq);
int buffer_tailq_empty(buffer_tailq_t *tailq);

#define BUFFER_TAILQ_PUSH_BACK(tailq, data) \
    buffer_tailq_push_back(tailq, data, sizeof(data))
#define BUFFER_TAILQ_PUSH_BACK_STRING(tailq, data) \
    buffer_tailq_push_back(tailq, data, sizeof(data) - 1)

#endif
