#include <arpa/inet.h>
#include <assert.h>

#include "log.h"
#include "protocol.h"
#include "transaction.h"

typedef enum write_status {
    WRITE_NOT_STARTED,
    WRITE_DONE,
    WRITE_WAIT,
    WRITE_ERROR
} write_status_t;

static void free_value(char **value)
{
    if (*value != NULL) {
        free(*value);
        *value = NULL;
    }
}

static void free_domain(transaction_t *transaction)
{
    free_value(&transaction->__domain);
}

static void free_reverse_path(transaction_t *transaction)
{
    free_value(&transaction->__reverse_path);
}

static void destroy_recipient_list(transaction_t *transaction)
{
    recipient_list_entry_t *item, *temp;
    LIST_FOREACH_SAFE(item, &transaction->__recipient_list, entry, temp) {
        LIST_REMOVE(item, entry);
        free(item->recipient.address);
        free(item);
    }
}

static void abort_write(transaction_t *transaction)
{
    if (close(transaction->__aiocb.aio_fildes) < 0) {
        CALL_ERR("close")
    }

    transaction->__aiocb.aio_fildes = -1;

    recipient_t *recipient = transaction->__first_recipient;
    maildir_t *maildir = &recipient->maildir;

    maildir_remove_file(maildir, transaction->__data_filename);
}

static int generate_filename(transaction_t *transaction)
{
    struct timeval timeval;

    if (gettimeofday(&timeval, NULL) < 0) {
        CALL_ERR("gettimeofday");
        return -1;
    }

    const size_t max_len = sizeof(transaction->__data_filename) - 1;
    char *name = transaction->__data_filename;
    char hostname[256];

    if (gethostname(hostname, sizeof(hostname) - 1) < 0) {
        CALL_ERR("gethostname");
        return -1;
    }

    if (snprintf(name, max_len, "%016lx_%016lx_%08x_%08x_%s.eml", timeval.tv_sec,
            timeval.tv_usec, getpid(), rand(), hostname) < 0) {
        CALL_ERR("snprintf");
        return -1;
    }

    return 0;
}

static int create_file(transaction_t *transaction)
{
    if (generate_filename(transaction) < 0) {
        return -1;
    }

    recipient_t *recipient = transaction->__first_recipient;
    maildir_t *maildir = &recipient->maildir;

    if (maildir_init(maildir, transaction->settings->maildir,
            recipient->address) < 0) {
        return -1;
    }

    return maildir_create_file(maildir, transaction->__data_filename);
}

static int continue_dump_data(transaction_t *transaction, const char *value,
    const size_t size)
{
    transaction->__aiocb.aio_buf = (void *) value;
    transaction->__aiocb.aio_offset += transaction->__aiocb.aio_nbytes;
    transaction->__aiocb.aio_nbytes = size;
    transaction->__aiocb.aio_lio_opcode = LIO_WRITE;

    if (aio_write(&transaction->__aiocb) < 0) {
        CALL_ERR("aio_write")
        return -1;
    }

    return 0;
}

static int begin_dump_data(transaction_t *transaction, const char *value,
    const size_t size)
{
    const int fd = create_file(transaction);

    if (fd < 0) {
        return -1;
    }

    transaction->__aiocb.aio_fildes = fd;
    transaction->__aiocb.aio_offset = 0;
    transaction->__aiocb.aio_nbytes = 0;

    return continue_dump_data(transaction, value, size);
}

static write_status_t get_write_status(transaction_t *transaction)
{
    if (-1 == transaction->__aiocb.aio_fildes) {
        return WRITE_NOT_STARTED;
    }

    switch (aio_error(&transaction->__aiocb)) {
        case 0: {
            if (NULL == transaction->__aiocb.aio_buf) {
                return WRITE_DONE;
            }

            const ssize_t result = aio_return(&transaction->__aiocb);

            if (result < transaction->__aiocb.aio_nbytes) {
                PRINT_STDERR("written %ld of %lu bytes", result, transaction->__aiocb.aio_nbytes);
                return WRITE_ERROR;
            }

            transaction->__aiocb.aio_buf = NULL;

            return WRITE_DONE;
        }

        case EINPROGRESS:
            return WRITE_WAIT;

        default:
            PRINT_STDERR("error in aio_error: %s", strerror(aio_error(&transaction->__aiocb)));
            return WRITE_ERROR;
    }
}

static void cancel_dump_data(transaction_t *transaction)
{
    switch (get_write_status(transaction)) {
        case WRITE_NOT_STARTED:
            break;

        case WRITE_WAIT:
            if (aio_cancel(transaction->__aiocb.aio_fildes, &transaction->__aiocb) < 0) {
                CALL_ERR("aio_cancel");
            }

        case WRITE_DONE:
            abort_write(transaction);
            break;

        default:
            break;
    }
}

static transaction_status_t async_dump_data(transaction_t *transaction,
    const char *value, const size_t size)
{
    switch (get_write_status(transaction)) {
        case WRITE_NOT_STARTED:
            if (begin_dump_data(transaction, value, size) < 0) {
                return TRANSACTION_ERROR;
            }
            return TRANSACTION_DONE;

        case WRITE_DONE:
            if (continue_dump_data(transaction, value, size) < 0) {
                return TRANSACTION_ERROR;
            }
            return TRANSACTION_DONE;

        case WRITE_WAIT:
            return TRANSACTION_WAIT;

        default:
            abort_write(transaction);
            return TRANSACTION_ERROR;
    }

    return 0;
}

static int end_write(transaction_t *transaction)
{
    if (close(transaction->__aiocb.aio_fildes) < 0) {
        PRINT_STDERR("error close file: %s", transaction->__data_filename);
        return -1;
    }

    transaction->__aiocb.aio_fildes = -1;

    return 0;
}

static int get_hostname(const int sock, char *hostname, const size_t size,
    int getname(int, struct sockaddr *, socklen_t *))
{
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);

    if (getname(sock, (struct sockaddr *) &addr, &addr_size) < 0) {
        CALL_ERR_ARGS("get*name", "%d", sock);
        return -1;
    }

    if (inet_ntop(AF_INET, &addr.sin_addr, hostname, size - 1) == NULL) {
        CALL_ERR("inet_ntop");
        return -1;
    }

    return 0;
}

static int get_local_hostname(const int sock, char *hostname, const size_t size)
{
    return get_hostname(sock, hostname, size, getsockname);
}

static int get_remote_hostname(const int sock, char *hostname, const size_t size)
{
    return get_hostname(sock, hostname, size, getpeername);
}

static void free_header(transaction_t *transaction)
{
    if (transaction->__header != NULL) {
        free(transaction->__header);
        transaction->__header = NULL;
    }
}

#define RETURN_PATH_HEADER "Return-path: <%s>" CRLF

#define RECIEVED_HEADER_FROM " from %s(%s)"
#define RECIEVED_HEADER_BY " by %s(%s)"
#define RECIEVED_HEADER_VIA " via %s"
#define RECIEVED_HEADER_FOR " for <%s>"
#define RECIEVED_HEADER_DATE "; %s"
#define RECIEVED_HEADER "Received:" \
    RECIEVED_HEADER_FROM \
    RECIEVED_HEADER_BY \
    RECIEVED_HEADER_VIA \
    RECIEVED_HEADER_FOR \
    RECIEVED_HEADER_DATE \
    CRLF

static char *generate_header(transaction_t *transaction)
{
    char from_tcp[100];
    if (get_remote_hostname(transaction->__sock, from_tcp, sizeof(from_tcp)) < 0) {
        return NULL;
    }

    char by_tcp[100];
    if (get_local_hostname(transaction->__sock, by_tcp, sizeof(by_tcp)) < 0) {
        return NULL;
    }

    char by_domain[256];
    if (gethostname(by_domain, sizeof(by_domain) - 1) < 0) {
        CALL_ERR("gethostname");
        return NULL;
    }

    const char *from_domain = transaction->__domain ? transaction->__domain : "";
    const char *return_path = transaction->__reverse_path;
    static const char via[] = "SMTP";
    const char *for_ = transaction->__first_recipient->address;

    struct timeval timeval;

    if (gettimeofday(&timeval, NULL) < 0) {
        CALL_ERR("gettimeofday");
        return NULL;
    }

    struct tm *tm = localtime(&timeval.tv_sec);

    if (NULL == tm) {
        CALL_ERR("localtime");
        return NULL;
    }

    char date[32];

    if (strftime(date, sizeof(date), "%a, %d %b %Y %T %z", tm) == 0) {
        CALL_ERR("strftime");
        return NULL;
    }

    const size_t size = sizeof(RECIEVED_HEADER) + sizeof(RETURN_PATH_HEADER)
        + strlen(return_path) + strlen(from_domain) + strlen(from_tcp)
        + strlen(by_domain) + strlen(by_tcp) + sizeof(via) + strlen(for_)
        + strlen(date);
    char *header = malloc(size);

    if (NULL == header) {
        CALL_ERR_ARGS("malloc", "%lu", size);
        return NULL;
    }

    if (snprintf(header, size, RETURN_PATH_HEADER RECIEVED_HEADER,
            return_path, from_domain, from_tcp, by_domain, by_tcp, via, for_,
            date) < 0) {
        free(header);
        CALL_ERR_ARGS("snprintf", "%s %lu %s %s %s %s %s %s %s %s",
            RETURN_PATH_HEADER RECIEVED_HEADER, size, return_path, from_domain,
            from_tcp, by_domain, by_tcp, via, for_, date);
        return NULL;
    }

    return header;
}

int transaction_init(transaction_t *transaction, const settings_t *settings,
    log_t *log, const int sock)
{
    transaction->settings = settings;
    transaction->log = log;
    transaction->__sock = sock;
    transaction->__domain = NULL;
    transaction->__header = NULL;
    transaction->__reverse_path = NULL;
    transaction->__first_recipient = NULL;
    LIST_INIT(&transaction->__recipient_list);
    transaction->__is_active = 0;

    memset(&transaction->__aiocb, 0, sizeof(transaction->__aiocb));

    transaction->__aiocb.aio_fildes = -1;

    memset(transaction->__data_filename, 0, sizeof(transaction->__data_filename));

    return 0;
}

void transaction_destroy(transaction_t *transaction)
{
    cancel_dump_data(transaction);
    free_header(transaction);
    free_domain(transaction);
    free_reverse_path(transaction);
    destroy_recipient_list(transaction);
}

void transaction_rollback(transaction_t *transaction)
{
    cancel_dump_data(transaction);
    free_header(transaction);
    free_reverse_path(transaction);
    destroy_recipient_list(transaction);

    LIST_INIT(&transaction->__recipient_list);

    transaction_reset_data(transaction);

    transaction->__is_active = 0;
}

static int set_value(char **dst, const char *value, const size_t length)
{
    assert(NULL == *dst);

    char *copy = malloc(length + 1);

    if (NULL == copy) {
        CALL_ERR_ARGS("malloc", "%lu", length + 1);
        return -1;
    }

    memcpy(copy, value, length);
    copy[length] = '\0';
    *dst = copy;

    return 0;
}

int transaction_set_domain(transaction_t *transaction, const char *value,
    const size_t length)
{
    free_domain(transaction);
    return set_value(&transaction->__domain, value, length);
}

int transaction_set_reverse_path(transaction_t *transaction, const char *value,
    const size_t length)
{
    return set_value(&transaction->__reverse_path, value, length);
}

int transaction_add_forward_path(transaction_t *transaction, const char *value,
    const size_t length)
{
    recipient_list_entry_t *item = malloc(sizeof(recipient_list_entry_t));

    if (NULL == item) {
        CALL_ERR_ARGS("malloc", "%lu", sizeof(recipient_list_entry_t));
        return -1;
    }

    char *forward_path = malloc(length + 1);

    if (NULL == forward_path) {
        CALL_ERR_ARGS("malloc", "%lu", length + 1);
        free(item);
        return -1;
    }

    memcpy(forward_path, value, length);
    forward_path[length] = '\0';

    item->recipient.address = forward_path;

    LIST_INSERT_HEAD(&transaction->__recipient_list, item, entry);

    if (NULL == transaction->__first_recipient) {
        transaction->__first_recipient = &item->recipient;
    }

    return 0;
}

void transaction_reset_data(transaction_t *transaction)
{
    if (transaction->__aiocb.aio_fildes != -1) {
        abort_write(transaction);
    }

    transaction->__aiocb.aio_fildes = -1;
    transaction->__aiocb.aio_buf = NULL;
    transaction->__aiocb.aio_nbytes = 0;
    transaction->__aiocb.aio_lio_opcode = LIO_NOP;

    memset(transaction->__data_filename, 0, sizeof(transaction->__data_filename));
}

ssize_t transaction_add_data(transaction_t *transaction, const char *value,
    const size_t size)
{
    switch (async_dump_data(transaction, value, size)) {
        case TRANSACTION_DONE:
            return size;
        case TRANSACTION_WAIT:
            return 0;
        case TRANSACTION_ERROR:
            break;
    }

    return -1;
}

transaction_status_t transaction_add_data_status(transaction_t *transaction)
{
    switch (get_write_status(transaction)) {
        case WRITE_NOT_STARTED:
        case WRITE_DONE:
            return TRANSACTION_DONE;
        case WRITE_WAIT:
            return TRANSACTION_WAIT;
        default:
            return TRANSACTION_ERROR;
    }
}

int transaction_add_header(transaction_t *transaction)
{
    char *header = generate_header(transaction);

    if (NULL == header) {
        return -1;
    }

    free_header(transaction);
    transaction->__header = header;

    if (transaction_add_data(transaction, header, strlen(header)) < 0) {
        return -1;
    }

    return 0;
}

int transaction_begin(transaction_t *transaction)
{
    assert(!transaction->__is_active);

    free_header(transaction);
    free_reverse_path(transaction);
    destroy_recipient_list(transaction);

    LIST_INIT(&transaction->__recipient_list);

    transaction_reset_data(transaction);

    transaction->__first_recipient = NULL;
    transaction->__is_active = 1;

    return 0;
}

transaction_status_t transaction_commit(transaction_t *transaction)
{
    if (!transaction->__is_active) {
        return TRANSACTION_ERROR;
    }

    switch (get_write_status(transaction)) {
        case WRITE_DONE: {
            if (end_write(transaction) < 0) {
                return TRANSACTION_ERROR;
            }

            struct recipient *recipient = transaction->__first_recipient;
            maildir_t *maildir = &recipient->maildir;

            if (maildir_move_to_new(maildir, transaction->__data_filename) < 0) {
                return TRANSACTION_ERROR;
            }

            const char *maildir_path = transaction->settings->maildir;

            recipient_list_entry_t *item, *temp;
            LIST_FOREACH_SAFE(item, &transaction->__recipient_list, entry, temp) {
                recipient_t *current = &item->recipient;
                if (current != recipient) {
                    if (maildir_init(&current->maildir, maildir_path, current->address) < 0) {
                        return TRANSACTION_ERROR;
                    }

                    if (maildir_clone_file(maildir, &current->maildir, transaction->__data_filename) < 0) {
                        return TRANSACTION_ERROR;
                    }
                }
            }

            transaction->__is_active = 0;

            return TRANSACTION_DONE;
        }
        case WRITE_WAIT:
            return TRANSACTION_WAIT;

        default:
            abort_write(transaction);
            return TRANSACTION_ERROR;
    }
}

int transaction_is_active(const transaction_t *transaction)
{
    return transaction->__is_active;
}

const char *transaction_data_filename(const transaction_t *transaction)
{
    return transaction->__data_filename;
}
