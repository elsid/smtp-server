#ifndef SMTP_SERVER_TIME
#define SMTP_SERVER_TIME

#include <sys/time.h>

struct timeval *timeval_diff(struct timeval *diff, struct timeval *start,
    struct timeval *end);
long long timeval_to_usec(struct timeval *value);
long long timeval_to_msec(struct timeval *value);
long long utimeval_diff(struct timeval *start, struct timeval *end);
long long mtimeval_diff(struct timeval *start, struct timeval *end);

#endif
