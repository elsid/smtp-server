#ifndef SMTP_SERVER_PARSE_H
#define SMTP_SERVER_PARSE_H

#include <pcre.h>

#include "buffer.h"
#include "protocol.h"

#define CRLF "\r\n"
#define RE_DOMAIN "[^/]+"
#define RE_EHLO_HELO "^(?i:" EHLO "|" HELO ")(?:\\s*(" RE_DOMAIN "))?\\s*" CRLF
#define RE_REVERSE_PATH "[^>]+"
#define RE_MAIL "^(?i:" MAIL "\\s+from):\\s*<(?:[^:]*:)?(" RE_REVERSE_PATH ")>.*" CRLF
#define RE_FORWARD_PATH RE_REVERSE_PATH
#define RE_RCPT "^(?i:" RCPT "\\s+to):\\s*<(?:[^:]*:)?(" RE_FORWARD_PATH ")>.*" CRLF

const char *parse_ehlo_helo(buffer_t *in_buf, size_t *length);
const char *parse_mail(buffer_t *in_buf, size_t *length);
const char *parse_rcpt(buffer_t *in_buf, size_t *length);

#endif
