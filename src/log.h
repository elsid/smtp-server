#ifndef SMTP_SERVER_LOG_H
#define SMTP_SERVER_LOG_H

#include <errno.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 4096
#define PATH_SIZE 256

typedef struct log {
    const char *__file_name;
    char __queue_name[PATH_SIZE];
    pid_t __writer;
    mqd_t __mq;
} log_t;

int log_init(log_t *log, const char *file_name);
void log_destroy(log_t *log);
int log_open(log_t *log);
int log_close(log_t *log);
int log_write(log_t *log, const char *format, ...);

#define PRINT_STDERR(format, ...) \
    if (fprintf(stderr, "[%d] [%s] " format "\n", getpid(), __func__, __VA_ARGS__) < 0) { \
        abort(); \
    }

#define CALL_ERR(function) \
    PRINT_STDERR("error in %s: %s", function, strerror(errno))

#define CALL_ERR_ARGS(function, args_format, ...) \
    PRINT_STDERR("error in %s(" args_format "): %s", function, __VA_ARGS__, strerror(errno))

#endif
