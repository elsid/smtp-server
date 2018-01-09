#include <pcre.h>
#include <sys/param.h>

#include "log.h"
#include "parse.h"

static const char *parse_first_group(const char *pattern, buffer_t *in_buf,
    size_t *length)
{
    const char *error;
    int error_offset;
    pcre *regexp = pcre_compile(pattern, 0, &error, &error_offset, NULL);

    if (NULL == regexp) {
        CALL_ERR_ARGS("pcre_compile", "%s at %d", error, error_offset);
        return NULL;
    }

    static const int offset_size = 6;
    static const size_t group1_begin = 2;
    static const size_t group1_end = 3;
    int offset[offset_size];
    const char *command = buffer_read_begin(in_buf);
    const char *begin = buffer_find(in_buf, CRLF, sizeof(CRLF) - 1);
    const size_t command_size = begin == buffer_end(in_buf) ? buffer_left(in_buf) : MIN(begin - command + sizeof(CRLF) - 1, INT_MAX);
    const int count = pcre_exec(regexp, NULL, command, (int) command_size, 0, 0, offset, offset_size);

    if (count != 2) {
        pcre_free(regexp);
        return NULL;
    }

    if (length != NULL) {
        *length = offset[group1_end] - offset[group1_begin];
    }

    pcre_free(regexp);

    return command + offset[group1_begin];
}

const char *parse_ehlo_helo(buffer_t *in_buf, size_t *length)
{
    return parse_first_group(RE_EHLO_HELO, in_buf, length);
}

const char *parse_mail(buffer_t *in_buf, size_t *length)
{
    return parse_first_group(RE_MAIL, in_buf, length);
}

const char *parse_rcpt(buffer_t *in_buf, size_t *length)
{
    return parse_first_group(RE_RCPT, in_buf, length);
}
