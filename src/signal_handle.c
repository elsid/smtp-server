#include <sys/wait.h>

#include "log.h"
#include "signal_handle.h"

int set_signal_handle(const int signum, void (*handle)(int))
{
    struct sigaction act;

    sigemptyset(&act.sa_mask);
    act.sa_handler = handle;
    act.sa_flags = 0;

    if (sigaction(signum, &act, NULL) < 0) {
        CALL_ERR("sigaction");
        return -1;
    }

    return 0;
}

int set_signals_handle(void (*handle)(int))
{
    const int signals[] = {SIGHUP, SIGINT, SIGPIPE, SIGTERM};
    const size_t count = sizeof(signals) / sizeof(int);

    for (size_t i = 0; i < count; ++i) {
        if (set_signal_handle(signals[i], handle) < 0) {
            return -1;
        }
    }

    return 0;
}
