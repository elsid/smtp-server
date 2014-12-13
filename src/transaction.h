#ifndef SMTP_SERVER_TRANSACTION_H
#define SMTP_SERVER_TRANSACTION_H

#include <aio.h>

#include "buffer.h"
#include "log.h"
#include "maildir.h"
#include "settings.h"

typedef LIST_HEAD(recipient_list, recipient_list_entry) recipient_list_t;

typedef struct recipient {
    char *address;
    maildir_t maildir;
} recipient_t;

typedef struct recipient_list_entry {
   recipient_t recipient;
   LIST_ENTRY(recipient_list_entry) entry;
} recipient_list_entry_t;

typedef struct transaction {
    int sock;
    const settings_t *settings;
    char *__domain;
    char *__reverse_path;
    struct recipient *first_recipient;
    log_t *log;
    int is_active;
    recipient_list_t __recipient_list;
    char data_filename[256 + 36];
    struct aiocb __aiocb;
    char *__header;
} transaction_t;

typedef enum transaction_status {
    TRANSACTION_DONE,
    TRANSACTION_WAIT,
    TRANSACTION_ERROR
} transaction_status_t;

int transaction_init(transaction_t *transaction, const settings_t *settings,
    log_t *log, const int sock);
void transaction_destroy(transaction_t *transaction);
void transaction_rollback(transaction_t *transaction);
int transaction_set_domain(transaction_t *transaction, const char *value,
    const size_t length);
int transaction_set_reverse_path(transaction_t *transaction, const char *value,
    const size_t length);
int transaction_add_forward_path(transaction_t *transaction, const char *value,
    const size_t length);
void transaction_reset_data(transaction_t *transaction);
ssize_t transaction_add_data(transaction_t *transaction,
    const char *value, const size_t size);
transaction_status_t transaction_add_data_status(transaction_t *transaction);
int transaction_add_header(transaction_t *transaction);
int transaction_begin(transaction_t *transaction);
transaction_status_t transaction_commit(transaction_t *transaction);

#endif
