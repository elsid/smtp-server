#include <ctype.h>
#include <sys/param.h>

#include "fsm.h"
#include "log.h"
#include "protocol.h"
#include "time.h"

const char *event_string(const te_smtp_server_event event)
{
    switch (event) {
        case SMTP_SERVER_EV_BEGIN:
            return "begin";
        case SMTP_SERVER_EV_RSET:
            return "rset";
        case SMTP_SERVER_EV_EHLO:
            return "ehlo";
        case SMTP_SERVER_EV_MAIL:
            return "mail";
        case SMTP_SERVER_EV_RCPT:
            return "rcpt";
        case SMTP_SERVER_EV_DATA:
            return "data";
        case SMTP_SERVER_EV_MORE_DATA:
            return "more_data";
        case SMTP_SERVER_EV_DATA_END:
            return "data_end";
        case SMTP_SERVER_EV_QUIT:
            return "quit";
        case SMTP_SERVER_EV_INVALID:
            return "invalid";
        default:
            return "undefined";
    }
}

const char *state_string(const te_smtp_server_state state)
{
    switch (state) {
        case SMTP_SERVER_ST_INIT:
            return "init";
        case SMTP_SERVER_ST_WAIT_EHLO:
            return "wait_ehlo";
        case SMTP_SERVER_ST_WAIT_MAIL:
            return "wait_mail";
        case SMTP_SERVER_ST_WAIT_RCPT:
            return "wait_rcpt";
        case SMTP_SERVER_ST_WAIT_RCPT_OR_DATA:
            return "wait_rcpt_or_data";
        case SMTP_SERVER_ST_WAIT_MORE_DATA:
            return "wait_more_data";
        case SMTP_SERVER_ST_ERROR:
            return "error";
        case SMTP_SERVER_ST_INVALID:
            return "invalid";
        case SMTP_SERVER_ST_DONE:
            return "done";
        default:
            return "undefined";
    }
}

static void string_to_lower(char *str, size_t max_length)
{
    for (; 0 != max_length && '\0' != *str; ++str, --max_length) {
        *str = tolower(*str);
    }
}

static int handle(context_t *context, te_smtp_server_event event)
{
    context->state = smtp_server_step(context->state, event, context);

    if (SMTP_SERVER_ST_ERROR == context->state) {
        BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue, "451" CRLF);
        return -1;
    }

    return 0;
}

static int handle_begin(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_BEGIN);
}

static int handle_ehlo(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_EHLO);
}

static int handle_mail(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_MAIL);
}

static int handle_rcpt(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_RCPT);
}

static int handle_data(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_DATA);
}

static int handle_more_data(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_MORE_DATA);
}

static int handle_data_end(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_DATA_END);
}

static int handle_rset(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_RSET);
}

static int handle_vrfy(context_t *context)
{
    if (buffer_shift_read_after(&context->in_message, CRLF) < 0) {
        return -1;
    }

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
            "502 Command not implemented" CRLF) < 0) {
        return -1;
    }

    return 0;
}

static int handle_noop(context_t *context)
{
    if (buffer_shift_read_after(&context->in_message, CRLF) < 0) {
        return -1;
    }

    return BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue, "250 Ok" CRLF);
}

static int handle_quit(context_t *context)
{
    return handle(context, SMTP_SERVER_EV_QUIT);
}

static int handle_wrong_command_sequence(context_t *context)
{
    log_write(context->log, "[%s] received wrong command %s at state %s",
        context->uuid, context->command, state_string(context->state));

    if (buffer_shift_read_after(&context->in_message, CRLF) < 0) {
        return -1;
    }

    return BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue, "503 Bad sequence of commands" CRLF);
}

static int handle_unrecognized_command(context_t *context)
{
    log_write(context->log, "[%s] received unrecognized command %s at state %s",
        context->uuid, context->command, state_string(context->state));

    if (buffer_shift_read_after(&context->in_message, CRLF) < 0) {
        return -1;
    }

    return BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
        "500 Syntax error, command unrecognized" CRLF);
}

typedef struct command {
    const char *name;
    int (*handle)(context_t *context);
} command_t;

static int process_command(context_t *context,
    const command_t *correct_commands, const command_t *correct_commands_end,
    const char **wrong_commands, const char **wrong_commands_end)
{
    for (const command_t *cur = correct_commands; cur < correct_commands_end; ++cur) {
        if (strncmp(context->command, cur->name, strlen(cur->name)) == 0) {
            log_write(context->log, "[%s] received correct command %s at state %s",
                context->uuid, context->command, state_string(context->state));
            return (*cur->handle)(context);
        }
    }

    for (const char **cur = wrong_commands; cur < wrong_commands_end; ++cur) {
        if (strncmp(context->command, *cur, strlen(*cur)) == 0) {
            return handle_wrong_command_sequence(context);
        }
    }

    return handle_unrecognized_command(context);
}

#define COMMANDS_END(commands) (commands + sizeof(commands) / sizeof(*commands))

static int process_command_on_wait_ehlo(context_t *context)
{
    static const command_t correct_commands[] = {
        {EHLO, handle_ehlo},
        {HELO, handle_ehlo},
        {NOOP, handle_noop},
        {RSET, handle_rset}
    };

    static const char *wrong_commands[] = {
        DATA,
        DATA_END,
        MAIL,
        QUIT,
        RCPT,
        VRFY
    };

    return process_command(context,
        correct_commands, COMMANDS_END(correct_commands),
        wrong_commands, COMMANDS_END(wrong_commands));
}

static int process_command_on_wait_mail(context_t *context)
{
    static const command_t correct_commands[] = {
        {EHLO, handle_ehlo},
        {HELO, handle_ehlo},
        {MAIL, handle_mail},
        {NOOP, handle_noop},
        {QUIT, handle_quit},
        {RSET, handle_rset},
        {VRFY, handle_vrfy}
    };

    static const char *wrong_commands[] = {
        DATA,
        DATA_END,
        RCPT
    };

    return process_command(context,
        correct_commands, COMMANDS_END(correct_commands),
        wrong_commands, COMMANDS_END(wrong_commands));
}

static int process_command_on_wait_rcpt(context_t *context)
{
    static const command_t correct_commands[] = {
        {EHLO, handle_ehlo},
        {HELO, handle_ehlo},
        {NOOP, handle_noop},
        {QUIT, handle_quit},
        {RCPT, handle_rcpt},
        {RSET, handle_rset},
        {VRFY, handle_vrfy}
    };

    static const char *wrong_commands[] = {
        DATA,
        DATA_END,
        MAIL
    };

    return process_command(context,
        correct_commands, COMMANDS_END(correct_commands),
        wrong_commands, COMMANDS_END(wrong_commands));
}

static int process_command_on_wait_rcpt_or_data(context_t *context)
{
    static const command_t correct_commands[] = {
        {DATA, handle_data},
        {EHLO, handle_ehlo},
        {HELO, handle_ehlo},
        {NOOP, handle_noop},
        {QUIT, handle_quit},
        {RCPT, handle_rcpt},
        {RSET, handle_rset},
        {VRFY, handle_vrfy}
    };

    static const char *wrong_commands[] = {
        DATA_END,
        MAIL
    };

    return process_command(context,
        correct_commands, COMMANDS_END(correct_commands),
        wrong_commands, COMMANDS_END(wrong_commands));
}

static int process_command_on_wait_more_data(context_t *context)
{
    if (strncmp(context->command, DATA_END, sizeof(DATA_END) - 1) == 0) {
        return handle_data_end(context);
    } else {
        return handle_more_data(context);
    }
}

static int process_command_on_error(context_t *context)
{
    static const command_t correct_commands[] = {
        {EHLO, handle_ehlo},
        {HELO, handle_ehlo},
        {NOOP, handle_noop},
        {QUIT, handle_quit},
        {RSET, handle_rset}
    };

    static const char *wrong_commands[] = {
        DATA,
        DATA_END,
        MAIL,
        RCPT,
        VRFY
    };

    return process_command(context,
        correct_commands, COMMANDS_END(correct_commands),
        wrong_commands, COMMANDS_END(wrong_commands));
}

int process_client(context_t *context)
{
    switch (context->state) {
        case SMTP_SERVER_ST_INIT:
            return handle_begin(context);
        case SMTP_SERVER_ST_DONE:
            return 0;
        default:
            break;
    }

    struct timeval current_time;

    if (gettimeofday(&current_time, NULL) < 0) {
        CALL_ERR("gettimeofday");
        return 0;
    }

    struct timeval diff;
    timeval_diff(&diff, &context->last_action_time, &current_time);
    const long long duration = timeval_to_msec(&diff);

    if (duration > context->settings->timeout) {
        log_write(context->log, "[%s] timeout after %ld msec",
            context->uuid, duration);
        return handle(context, SMTP_SERVER_EV_TIMEOUT);
    }

    if (!context->is_wait_transition) {
        buffer_t *in_buf = &context->in_message;

        if (buffer_left(in_buf) == 0) {
            return 0;
        }

        const char *end = buffer_find(in_buf, CRLF);

        if (end == buffer_end(in_buf)) {
            return 0;
        }

        char *command_begin = buffer_read_begin(in_buf);

        while (command_begin < end
                && (' ' == *command_begin || '\t' == *command_begin
                || '\r' == *command_begin || '\n' == *command_begin)) {
            ++command_begin;
        }

        buffer_shift_read(in_buf, command_begin - buffer_read_begin(in_buf));

        const char *command_end = MIN(buffer_find(in_buf, " ") + 1, end + 2);
        const size_t command_size = MIN(command_end - command_begin,
            sizeof(context->command) - 1);

        strncpy(context->command, command_begin, command_size);
        context->command[command_size] = '\0';
        string_to_lower(context->command, command_size);
    }

    if (gettimeofday(&context->last_action_time, NULL) < 0) {
        CALL_ERR("gettimeofday");
        return -1;
    }

    switch (context->state) {
        case SMTP_SERVER_ST_WAIT_EHLO:
            return process_command_on_wait_ehlo(context);
        case SMTP_SERVER_ST_WAIT_MAIL:
            return process_command_on_wait_mail(context);
        case SMTP_SERVER_ST_WAIT_RCPT:
            return process_command_on_wait_rcpt(context);
        case SMTP_SERVER_ST_WAIT_RCPT_OR_DATA:
            return process_command_on_wait_rcpt_or_data(context);
        case SMTP_SERVER_ST_WAIT_MORE_DATA:
            return process_command_on_wait_more_data(context);
        case SMTP_SERVER_ST_ERROR:
            return process_command_on_error(context);
        default:
            return -1;
    }
}
