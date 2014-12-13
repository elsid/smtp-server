#ifndef SMTP_SERVER_WORKER_H
#define SMTP_SERVER_WORKER_H

#include "log.h"
#include "settings.h"

typedef enum worker_status {
    WORKER_RUNNING,
    WORKER_ERROR
} worker_status_t;

typedef struct worker {
    pid_t pid;
    int sock;
    worker_status_t status;
} worker_t;

int worker_init(worker_t *worker, const settings_t *settings, log_t *log);
void worker_destroy(worker_t *worker);

#endif
