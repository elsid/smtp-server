#include "log.h"
#include "handle.h"
#include "protocol.h"
#include "parse.h"

transition_result_t handle_begin(context_t *context)
{
    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
            "220 Service ready" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    return TRANSITION_SUCCEED;
}

transition_result_t handle_ehlo(context_t *context)
{
    if (context->transaction.__is_active) {
        transaction_rollback(&context->transaction);
        log_write(context->log, "[%s] rollback transaction", context->uuid);
    }

    buffer_t *in_buf = &context->in_message;
    size_t domain_length;
    const char *domain = parse_ehlo_helo(in_buf, &domain_length);

    if (domain != NULL) {
        if (transaction_set_domain(&context->transaction, domain, domain_length) < 0) {
            return TRANSITION_ERROR;
        }

        log_write(context->log, "[%s] begin session with domain: %.*s",
            context->uuid, domain_length, domain);
    }

    if (buffer_shift_read_after(in_buf, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue, "250 Ok" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    return TRANSITION_SUCCEED;
}

transition_result_t handle_rset(context_t *context)
{
    if (transaction_is_active(&context->transaction)) {
        transaction_rollback(&context->transaction);
        log_write(context->log, "[%s] rollback transaction", context->uuid);
    }

    if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue, "250 Ok" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    return TRANSITION_SUCCEED;
}

transition_result_t handle_vrfy(context_t *context)
{
    if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
            "502 Command not implemented" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    return TRANSITION_SUCCEED;
}

transition_result_t handle_mail(context_t *context)
{
    buffer_t *in_buf = &context->in_message;
    size_t reverse_path_length;
    const char *reverse_path = parse_mail(in_buf, &reverse_path_length);

    if (NULL == reverse_path) {
        if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
            return TRANSITION_ERROR;
        }

        if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
                "555 Syntax error in reverse-path or not present" CRLF) < 0) {
            return TRANSITION_ERROR;
        }

        return TRANSITION_FAILED;
    }

    if (transaction_begin(&context->transaction) < 0) {
        return TRANSITION_ERROR;
    }

    if (transaction_set_reverse_path(&context->transaction, reverse_path,
            reverse_path_length) < 0) {
        CALL_ERR("transaction_set_reverse_path");
        return TRANSITION_ERROR;
    }

    if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue, "250 Ok" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    log_write(context->log, "[%s] begin transaction", context->uuid);

    return TRANSITION_SUCCEED;
}

transition_result_t handle_rcpt(context_t *context)
{
    buffer_t *in_buf = &context->in_message;
    size_t forward_path_length;
    const char *forward_path = parse_rcpt(in_buf, &forward_path_length);

    if (NULL == forward_path) {
        if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
            return TRANSITION_ERROR;
        }

        if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
                "555 Syntax error in forward-path or not present" CRLF) < 0) {
            return TRANSITION_ERROR;
        }

        return TRANSITION_FAILED;
    }

    if (transaction_add_forward_path(&context->transaction, forward_path,
            forward_path_length) < 0) {
        CALL_ERR("transaction_add_forward_path");
        return TRANSITION_ERROR;
    }

    if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue, "250 Ok" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    return TRANSITION_SUCCEED;
}

transition_result_t handle_data_begin(context_t *context)
{
    if (context->is_wait_transition) {
        switch (transaction_add_data_status(&context->transaction)) {
            case TRANSACTION_DONE:
                context->is_wait_transition = 0;
                if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
                        "354 Start mail input; end with <CRLF>.<CRLF>" CRLF) < 0) {
                    return TRANSITION_ERROR;
                }
                return TRANSITION_SUCCEED;
            case TRANSACTION_WAIT:
                return TRANSITION_WAIT;
            default:
                return TRANSITION_ERROR;
        }
    }

    if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    if (transaction_add_header(&context->transaction) < 0) {
        return TRANSITION_ERROR;
    }

    context->is_wait_transition = 1;

    return TRANSITION_WAIT;
}

transition_result_t handle_data(context_t *context)
{
    buffer_t *in_buf = &context->in_message;

    if (context->is_wait_transition) {
        switch (transaction_add_data_status(&context->transaction)) {
            case TRANSACTION_DONE:
                if (buffer_space(&context->in_message) == 0) {
                    buffer_drop_read(&context->in_message);
                }
                context->is_wait_transition = 0;
                return TRANSITION_SUCCEED;
            case TRANSACTION_WAIT:
                return TRANSITION_WAIT;
            default:
                return TRANSITION_ERROR;
        }
    }

    const char *begin = buffer_find(in_buf, CRLF, sizeof(CRLF) - 1);

    if (begin == buffer_end(in_buf)) {
        return TRANSITION_ERROR;
    }

    const char *data = buffer_read_begin(in_buf);
    const size_t data_size = begin - data + sizeof(CRLF) - 1;

    if (transaction_add_data(&context->transaction, data, data_size) != data_size) {
        return TRANSITION_ERROR;
    }

    buffer_shift_read(in_buf, data_size);

    context->is_wait_transition = 1;

    return TRANSITION_WAIT;
}

transition_result_t handle_data_end(context_t *context)
{
    switch (transaction_commit(&context->transaction)) {
        case TRANSACTION_DONE:
            break;
        case TRANSACTION_WAIT:
            context->is_wait_transition = 1;
            return TRANSITION_WAIT;
        default:
            return TRANSITION_ERROR;
    }

    context->is_wait_transition = 0;

    if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue, "250 Ok" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    log_write(context->log, "[%s] commit transaction, file saved: %s",
        context->uuid, context->transaction.__data_filename);

    return TRANSITION_SUCCEED;
}

transition_result_t handle_quit(context_t *context)
{
    if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
            "221 Service closing transmission channel" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    return TRANSITION_SUCCEED;
}

transition_result_t handle_invalid(context_t *context)
{
    if (buffer_shift_read_after(&context->in_message, CRLF, sizeof(CRLF) - 1) < 0) {
        return TRANSITION_ERROR;
    }

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    if (BUFFER_TAILQ_PUSH_BACK_STRING(&context->out_message_queue,
            "451 Requested action aborted: internal error" CRLF) < 0) {
        return TRANSITION_ERROR;
    }

    return TRANSITION_SUCCEED;
}
