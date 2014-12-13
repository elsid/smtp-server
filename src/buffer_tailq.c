#include <assert.h>

#include "buffer_tailq.h"
#include "log.h"

void buffer_tailq_init(buffer_tailq_t *tailq)
{
    assert(NULL != tailq);
    TAILQ_INIT(tailq);
}

void buffer_tailq_destroy(buffer_tailq_t *tailq)
{
    assert(NULL != tailq);

    buffer_tailq_entry_t *item, *temp;

    TAILQ_FOREACH_SAFE(item, tailq, entry, temp) {
        TAILQ_REMOVE(tailq, item, entry);
        buffer_destroy(&item->buffer);
        free(item);
    }
}

int buffer_tailq_push_back(buffer_tailq_t *tailq, const void *data,
    const size_t size)
{
    assert(NULL != tailq);
    assert(NULL != data);

    buffer_tailq_entry_t *item = malloc(sizeof(buffer_tailq_entry_t));

    if (NULL == item) {
        CALL_ERR_ARGS("malloc", "%lu", sizeof(buffer_tailq_entry_t));
        return -1;
    }

    if (buffer_init(&item->buffer, size) < 0) {
        return -1;
    }

    buffer_write(&item->buffer, data, size);

    TAILQ_INSERT_TAIL(tailq, item, entry);

    return 0;
}

int buffer_tailq_push_back_string(buffer_tailq_t *tailq, const char *string)
{
    assert(NULL != string);
    return buffer_tailq_push_back(tailq, string, strlen(string));
}

buffer_t *buffer_tailq_front(buffer_tailq_t *tailq)
{
    assert(NULL != tailq);
    return &TAILQ_FIRST(tailq)->buffer;
}

void buffer_tailq_pop_front(buffer_tailq_t *tailq)
{
    assert(!buffer_tailq_empty(tailq));
    buffer_tailq_entry_t *first = TAILQ_FIRST(tailq);
    TAILQ_REMOVE(tailq, first, entry);
    buffer_destroy(&first->buffer);
    free(first);
}

int buffer_tailq_empty(buffer_tailq_t *tailq)
{
    assert(NULL != tailq);
    return TAILQ_EMPTY(tailq);
}
