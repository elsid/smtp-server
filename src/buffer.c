#include <assert.h>

#include "buffer.h"
#include "log.h"

int buffer_init(buffer_t *buf, const size_t size)
{
    assert(NULL != buf);

    char *data = malloc(size);

    if (NULL == data) {
        CALL_ERR_ARGS("malloc", "%lu", size);
        return -1;
    }

    memset(data, 0, size);

    buf->data = data;
    buf->read_pos = 0;
    buf->write_pos = 0;
    buf->size = size;

    return 0;
}

void buffer_destroy(buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->data);
    free(buf->data);
}

void buffer_reset_read(buffer_t *buf)
{
    assert(NULL != buf);
    buf->read_pos = 0;
}

void buffer_reset_write(buffer_t *buf)
{
    assert(NULL != buf);
    buf->write_pos = 0;
}

void buffer_reset(buffer_t *buf)
{
    buffer_reset_read(buf);
    buffer_reset_write(buf);
}

int buffer_resize(buffer_t *buf, const size_t size)
{
    assert(NULL != buf);
    assert(buf->data);

    if (buf->read_pos > 0) {
        char *data = malloc(size);

        if (NULL == data) {
            CALL_ERR_ARGS("malloc", "%lu", size);
            return -1;
        }

        memcpy(data, buf->data + buf->read_pos, buf->size - buf->read_pos);
        memset(data + buf->size - buf->read_pos, 0, buf->read_pos);

        buf->data = data;
        buf->write_pos -= buf->read_pos;
        buf->read_pos = 0;
    } else {
        char *data = realloc(buf->data, size);

        if (NULL == data) {
            CALL_ERR_ARGS("realloc", "%p, %lu", buf->data, size);
            return -1;
        }

        buf->data = data;

        if (size < buf->read_pos) {
            buf->read_pos = size;
        }
    }

    if (size < buf->write_pos) {
        buf->write_pos = size;
    }

    buf->size = size;

    return 0;
}

char *buffer_begin(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->data);
    return buf->data;
}

char *buffer_read_begin(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->data);
    return buf->data + buf->read_pos;
}

char *buffer_write_begin(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->data);
    return buf->data + buf->write_pos;
}

char *buffer_end(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->data);
    return buf->data + buf->size;
}

size_t buffer_space(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->write_pos <= buf->size);
    return buf->size - buf->write_pos;
}

size_t buffer_left(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->read_pos <= buf->write_pos);
    return buf->write_pos - buf->read_pos;
}

void buffer_shift_read(buffer_t *buf, const size_t shift)
{
    assert(NULL != buf);
    assert(buf->size >= buf->read_pos + shift);
    buf->read_pos += shift;
}

void buffer_shift_write(buffer_t *buf, const size_t shift)
{
    assert(NULL != buf);
    assert(buf->size >= buf->write_pos + shift);
    buf->write_pos += shift;
}

void buffer_write(buffer_t *buf, const void *data, const size_t size)
{
    assert(NULL != buf);
    assert(buffer_space(buf) >= size);

    memcpy(buffer_write_begin(buf), data, size);
    buffer_shift_write(buf, size);
}

void buffer_write_string(buffer_t *buf, const char *string)
{
    assert(NULL != buf);
    buffer_write(buf, string, strlen(string));
}

char *buffer_find(const buffer_t *buf, const char *string)
{
    assert(NULL != buf);
    assert(buf->data);
    assert(buf->read_pos <= buf->write_pos);
    assert(NULL != string);

    const size_t len = strlen(string);

    if (len > buf->write_pos - buf->read_pos) {
        return buffer_end(buf);
    }

    const size_t end = buf->write_pos - len + 1;

    for (size_t i = buf->read_pos; i < end; i++) {
        size_t j = 0;

        while (j < len && buf->data[i + j] == string[j]) {
            ++j;
        }

        if (j == len) {
            return &buf->data[i];
        }
    }

    return buffer_end(buf);
}

int buffer_shift_read_after(buffer_t *buf, const char *string)
{
    assert(NULL != buf);

    const char *end = buffer_find(buf, string);

    if (end == buffer_end(buf)) {
        return -1;
    }

    buffer_shift_read(buf, end - buffer_read_begin(buf) + strlen(string));

    return 0;
}

void buffer_drop_read(buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->data);
    assert(buf->read_pos <= buf->write_pos);

    const size_t size = buf->write_pos - buf->read_pos;

    memmove(buf->data, buf->data + buf->read_pos, size);
    memset(buf->data + size, 0, buf->size - size);

    buf->read_pos = 0;
    buf->write_pos = size;
}
