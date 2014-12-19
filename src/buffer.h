#ifndef SMTP_SERVER_BUFFER_H
#define SMTP_SERVER_BUFFER_H

#include <sys/types.h>

typedef struct buffer {
    char *__data;
    size_t __read_pos;
    size_t __write_pos;
    size_t __size;
} buffer_t;

int buffer_init(buffer_t *buf, const size_t size);
void buffer_destroy(buffer_t *buf);
void buffer_reset_read(buffer_t *buf);
void buffer_reset_write(buffer_t *buf);
void buffer_reset(buffer_t *buf);
int buffer_resize(buffer_t *buf, const size_t size);
char *buffer_begin(const buffer_t *buf);
char *buffer_read_begin(const buffer_t *buf);
char *buffer_write_begin(const buffer_t *buf);
char *buffer_end(const buffer_t *buf);
size_t buffer_space(const buffer_t *buf);
size_t buffer_left(const buffer_t *buf);
void buffer_shift_read(buffer_t *buf, const size_t shift);
void buffer_shift_write(buffer_t *buf, const size_t shift);
void buffer_write(buffer_t *buf, const void *data, const size_t size);
void buffer_write_string(buffer_t *buf, const char *string);
char *buffer_find(const buffer_t *buf, const char *string);
int buffer_shift_read_after(buffer_t *buf, const char *string);
void buffer_drop_read(buffer_t *buf);

#endif
