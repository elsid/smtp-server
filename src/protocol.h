#ifndef SMTP_SERVER_PROTOCOL_H
#define SMTP_SERVER_PROTOCOL_H

#include "context.h"

#define CRLF "\r\n"
#define DATA "data"
#define DATA_END "." CRLF
#define EHLO "ehlo"
#define HELO "helo"
#define MAIL "mail"
#define NOOP "noop"
#define QUIT "quit"
#define RCPT "rcpt"
#define RSET "rset"
#define VRFY "vrfy"

const char *event_string(const te_smtp_server_event event);
const char *state_string(const te_smtp_server_state state);
int process_client(context_t *context);

#endif
