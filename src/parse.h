#ifndef SMTP_SERVER_PARSE_H
#define SMTP_SERVER_PARSE_H

#include <pcre.h>

#include "buffer.h"

const char *parse_domain(buffer_t *in_buf, size_t *length);
const char *parse_reverse_path(buffer_t *in_buf, size_t *length);
const char *parse_forward_path(buffer_t *in_buf, size_t *length);

#endif
