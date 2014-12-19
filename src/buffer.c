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

    buf->__data = data;
    buf->__read_pos = 0;
    buf->__write_pos = 0;
    buf->__size = size;

    return 0;
}

void buffer_destroy(buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->__data);
    free(buf->__data);
}

void buffer_reset_read(buffer_t *buf)
{
    assert(NULL != buf);
    buf->__read_pos = 0;
}

void buffer_reset_write(buffer_t *buf)
{
    assert(NULL != buf);
    buf->__write_pos = 0;
}

void buffer_reset(buffer_t *buf)
{
    buffer_reset_read(buf);
    buffer_reset_write(buf);
}

int buffer_resize(buffer_t *buf, const size_t size)
{
    assert(NULL != buf);
    assert(buf->__data);

    if (buf->__read_pos > 0) {
        char *data = malloc(size);

        if (NULL == data) {
            CALL_ERR_ARGS("malloc", "%lu", size);
            return -1;
        }

        memcpy(data, buf->__data + buf->__read_pos, buf->__size - buf->__read_pos);
        memset(data + buf->__size - buf->__read_pos, 0, buf->__read_pos);

        buf->__data = data;
        buf->__write_pos -= buf->__read_pos;
        buf->__read_pos = 0;
    } else {
        char *data = realloc(buf->__data, size);

        if (NULL == data) {
            CALL_ERR_ARGS("realloc", "%p, %lu", buf->__data, size);
            return -1;
        }

        buf->__data = data;

        if (size < buf->__read_pos) {
            buf->__read_pos = size;
        }
    }

    if (size < buf->__write_pos) {
        buf->__write_pos = size;
    }

    buf->__size = size;

    return 0;
}

char *buffer_begin(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->__data);
    return buf->__data;
}

char *buffer_read_begin(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->__data);
    return buf->__data + buf->__read_pos;
}

char *buffer_write_begin(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->__data);
    return buf->__data + buf->__write_pos;
}

char *buffer_end(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->__data);
    return buf->__data + buf->__size;
}

size_t buffer_space(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->__write_pos <= buf->__size);
    return buf->__size - buf->__write_pos;
}

size_t buffer_left(const buffer_t *buf)
{
    assert(NULL != buf);
    assert(buf->__read_pos <= buf->__write_pos);
    return buf->__write_pos - buf->__read_pos;
}

void buffer_shift_read(buffer_t *buf, const size_t shift)
{
    assert(NULL != buf);
    assert(buf->__size >= buf->__read_pos + shift);
    buf->__read_pos += shift;
}

void buffer_shift_write(buffer_t *buf, const size_t shift)
{
    assert(NULL != buf);
    assert(buf->__size >= buf->__write_pos + shift);
    buf->__write_pos += shift;
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
    assert(buf->__data);
    assert(buf->__read_pos <= buf->__write_pos);
    assert(NULL != string);

    const size_t len = strlen(string);

    if (len > buf->__write_pos - buf->__read_pos) {
        return buffer_end(buf);
    }

    const size_t end = buf->__write_pos - len + 1;

    for (size_t i = buf->__read_pos; i < end; i++) {
        size_t j = 0;

        while (j < len && buf->__data[i + j] == string[j]) {
            ++j;
        }

        if (j == len) {
            return &buf->__data[i];
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
    assert(buf->__data);
    assert(buf->__read_pos <= buf->__write_pos);

    const size_t size = buf->__write_pos - buf->__read_pos;

    memmove(buf->__data, buf->__data + buf->__read_pos, size);
    memset(buf->__data + size, 0, buf->__size - size);

    buf->__read_pos = 0;
    buf->__write_pos = size;
}
