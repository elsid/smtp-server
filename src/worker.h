#ifndef SMTP_SERVER_WORKER_H
#define SMTP_SERVER_WORKER_H

#include "log.h"
#include "settings.h"

typedef enum worker_status {
    WORKER_RUNNING,
    WORKER_ERROR
} worker_status_t;

typedef struct worker {
    pid_t __pid;
    int __sock;
    worker_status_t __status;
} worker_t;

int worker_init(worker_t *worker, const settings_t *settings, log_t *log);
void worker_destroy(worker_t *worker);
int worker_send_socket(worker_t *worker, const int sock);
worker_status_t worker_status(const worker_t *worker);

#endif
