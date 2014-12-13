#ifndef SMTP_SERVER_HANDLE_H
#define SMTP_SERVER_HANDLE_H

#include "context.h"
#include "transition_result.h"

transition_result_t handle_begin(context_t *context);
transition_result_t handle_ehlo(context_t *context);
transition_result_t handle_rset(context_t *context);
transition_result_t handle_vrfy(context_t *context);
transition_result_t handle_mail(context_t *context);
transition_result_t handle_rcpt(context_t *context);
transition_result_t handle_data_begin(context_t *context);
transition_result_t handle_data(context_t *context);
transition_result_t handle_data_end(context_t *context);
transition_result_t handle_quit(context_t *context);
transition_result_t handle_invalid(context_t *context);

#endif
