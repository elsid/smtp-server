#include <assert.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>

#include "log.h"
#include "signal_handle.h"

#define MQ_FLAGS 0
#define MAX_MESSAGE_COUNT 10

static void do_nothing(int signum) {}

static int set_log_signals_handle()
{
    return set_signals_handle(do_nothing);
}

static int dump(FILE *file, const char *message)
{
    if (fprintf(file, "%s", message) < 0) {
        CALL_ERR("fprintf");
        return -1;
    }

    fflush(file);

    return 0;
}

static int log_run(log_t *log)
{
    if (set_log_signals_handle() < 0) {
        return -1;
    }

    FILE *file = fopen(log->__file_name, "a+");

    if (NULL == file) {
        CALL_ERR("fopen");
        return -1;
    }

    struct mq_attr attr = {
        .mq_flags = MQ_FLAGS,
        .mq_maxmsg = MAX_MESSAGE_COUNT,
        .mq_msgsize = MAX_MESSAGE_SIZE,
        .mq_curmsgs = 0
    };

    const mqd_t mq = mq_open(log->__queue_name, O_CREAT | O_RDONLY, 0644, &attr);

    if (mq < 0) {
        if (fclose(file) < 0) {
            CALL_ERR("fclose");
        }

        CALL_ERR("mq_open");
        return -1;
    }

    int result = 0;
    char message[MAX_MESSAGE_SIZE];

    while (1) {
        ssize_t read_size = mq_receive(mq, message, sizeof(message), NULL);

        if (read_size < 0) {
            if (EINTR != errno) {
                CALL_ERR("mq_receive");
                break;
            }
        } else if (0 == read_size) {
            break;
        } else {
            if (dump(file, message) < 0) {
                break;
            }
        }
    }

    if (mq_close(mq) < 0) {
        CALL_ERR("mq_close");
        result = -1;
    }

    if (mq_unlink(log->__queue_name) < 0) {
        CALL_ERR("mq_unlink");
        result = -1;
    }

    if (fclose(file) < 0) {
        CALL_ERR("fclose");
        result = -1;
    }

    return result;
}

int log_init(log_t *log, const char *file_name)
{
    assert(NULL != log);

    struct timeval timeval;

    if (gettimeofday(&timeval, NULL) < 0) {
        CALL_ERR("gettimeofday");
        return -1;
    }

    if (snprintf(log->__queue_name, sizeof(log->__queue_name), "/smtp-server.log") < 0) {
        CALL_ERR("snprintf");
        return -1;
    }

    log->__file_name = file_name;

    pid_t writer = fork();

    if (writer < 0) {
        CALL_ERR("fork");
        return -1;
    } else if (0 == writer) {
        log->__writer = 0;
        log->__mq = -1;
        if (log_run(log) < 0) {
            return -1;
        }
        return 1;
    }

    log->__writer = writer;
    log->__mq = -1;

    return 0;
}

void log_destroy(log_t *log)
{
    if (mq_send(log->__mq, "", 0, 0) < 0) {
        CALL_ERR("mq_send");
    }

    log_close(log);

    if (waitpid(log->__writer, NULL, 0) < 0) {
        CALL_ERR("waitpid");
    }
}

int log_open(log_t *log)
{
    if (log->__mq != -1) {
        PRINT_STDERR("error: log is open %s", "");
        return -1;
    }

    struct mq_attr attr = {
        .mq_flags = MQ_FLAGS,
        .mq_maxmsg = MAX_MESSAGE_COUNT,
        .mq_msgsize = MAX_MESSAGE_SIZE,
        .mq_curmsgs = 0
    };

    const mqd_t mq = mq_open(log->__queue_name, O_CREAT | O_WRONLY, 0644, &attr);

    if (mq < 0) {
        CALL_ERR("mq_open");
        return -1;
    }

    log->__mq = mq;

    return 0;
}

int log_close(log_t *log)
{
    int result = 0;

    if (mq_close(log->__mq) < 0) {
        CALL_ERR("mq_close");
        result = -1;
    }

    if (mq_unlink(log->__queue_name) < 0) {
        CALL_ERR("mq_unlink");
        result = -1;
    }

    return result;
}

int log_write(log_t *log, const char *format, ...)
{
    assert(NULL != log);
    assert(NULL != format);

    va_list args;
    va_start(args, format);

    char user_message[MAX_MESSAGE_SIZE];

    if (vsnprintf(user_message, sizeof(user_message) - 1, format, args) < 0) {
        va_end(args);
        return -1;
    }

    va_end(args);

    struct timeval timeval;

    if (gettimeofday(&timeval, NULL) < 0) {
        CALL_ERR("gettimeofday");
        return -1;
    }

    struct tm *tm = localtime(&timeval.tv_sec);

    if (NULL == tm) {
        CALL_ERR("localtime");
        return -1;
    }

    char time_string[40];

    if (strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", tm) == 0) {
        CALL_ERR("strftime");
        return -1;
    }

    char log_message[MAX_MESSAGE_SIZE];

    if (snprintf(log_message, sizeof(log_message), "[%s.%06ld] [%d] %s\n",
            time_string, timeval.tv_usec, getpid(), user_message) < 0) {
        CALL_ERR("strftime");
        return -1;
    }

    if (mq_send(log->__mq, log_message, strlen(log_message) + 1, 0) < 0) {
        CALL_ERR("mq_send");
        return -1;
    }

    return 0;
}
