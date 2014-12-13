#include <math.h>

#include "time.h"

#define MICROSECONDS_IN_MILLISECOND 1000
#define MILLISECONDS_IN_SECOND 1000
#define MICROSECONDS_IN_SECOND (MICROSECONDS_IN_MILLISECOND * MILLISECONDS_IN_SECOND)

struct timeval *timeval_diff(struct timeval *diff, struct timeval *start,
    struct timeval *end)
{
    diff->tv_sec = end->tv_sec  - start->tv_sec;
    diff->tv_usec = end->tv_usec - start->tv_usec;

    if (diff->tv_usec < 0) {
        --diff->tv_sec;
        diff->tv_usec += MICROSECONDS_IN_SECOND;
    }

    return diff;
}

long long timeval_to_usec(struct timeval *value)
{
    return value->tv_sec * MICROSECONDS_IN_SECOND + value->tv_usec;
}

long long timeval_to_msec(struct timeval *value)
{
    return value->tv_sec * MILLISECONDS_IN_SECOND
        + (long long) ceil((double) value->tv_usec / MICROSECONDS_IN_MILLISECOND);
}

long long utimeval_diff(struct timeval *start, struct timeval *end)
{
    struct timeval diff;
    return timeval_to_usec(timeval_diff(&diff, start, end));
}

long long mtimeval_diff(struct timeval *start, struct timeval *end)
{
    struct timeval diff;
    return timeval_to_msec(timeval_diff(&diff, start, end));
}
