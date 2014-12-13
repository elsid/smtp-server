#ifndef SMTP_SERVER_SIGNAL_HANDLE_H
#define SMTP_SERVER_SIGNAL_HANDLE_H

int set_signal_handle(const int signum, void (*handle)(int));
int set_signals_handle(void (*handle)(int));

#endif
