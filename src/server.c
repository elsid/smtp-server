#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>

#include "log.h"
#include "settings.h"
#include "signal_handle.h"
#include "worker.h"

static volatile int done = 0;

static void set_done(int signum)
{
    if (0 == done && (SIGTERM == signum || SIGINT == signum)) {
        done = signum;
    }
}

static int set_server_signals_handle()
{
    return set_signals_handle(set_done);
}

static const char *wildcard(const char *address)
{
    if (NULL == address) {
        return NULL;
    }

    static const char as_null[] = {'\0', '-', '?', '*', ':'};

    if (memchr(as_null, address[0], sizeof(as_null)) != NULL) {
        return NULL;
    }

    return address;
}

typedef struct worker_pool {
    worker_t *workers;
    size_t size;
    size_t current;
    const settings_t *settings;
    log_t *log;
} worker_pool_t;

static int init_worker_pool(worker_pool_t *pool, const size_t size,
    const settings_t *settings, log_t *log)
{
    if (size == 0) {
        PRINT_STDERR("%s", "size == 0");
        return -1;
    }

    worker_t *workers = calloc(size, sizeof(worker_t));

    if (NULL == workers) {
        CALL_ERR("calloc");
        return -1;
    }

    for (size_t i = 0; i < size; ++i) {
        switch (worker_init(&workers[i], settings, log)) {
            case -1:
                for (size_t j = 0; j < i; ++j) {
                    worker_destroy(&workers[i]);
                    free(workers);
                    return -1;
                }
            case 0:
                break;
            case 1:
                free(workers);
                return 1;
        }
    }

    pool->workers = workers;
    pool->size = size;
    pool->current = size - 1;
    pool->settings = settings;
    pool->log = log;

    return 0;
}

static void free_worker_pool(worker_pool_t *pool)
{
    for (size_t i = 0; i < pool->size; ++i) {
        worker_destroy(&pool->workers[i]);
    }

    free(pool->workers);
}

static struct addrinfo *make_addrinfo_list(const char *addr, const uint16_t port)
{
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV | AI_ALL,
        .ai_protocol = 0,
        .ai_addrlen = 0,
        .ai_addr = NULL,
        .ai_canonname = NULL,
        .ai_next = NULL
    };
    struct addrinfo *list;
    char service[6];

    memset(service, 0, sizeof(service));
    sprintf(service, "%u", port);

    if (getaddrinfo(wildcard(addr), service, &hints, &list) < 0) {
        CALL_ERR("getaddrinfo");
        return NULL;
    }

    return list;
}

static int bind_socket(struct addrinfo *list)
{
    int sock = -1;

    for (struct addrinfo *curr = list; curr != NULL; curr = curr->ai_next) {
        sock = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

        if (sock < 0) {
            CALL_ERR("socket");
            continue;
        }

        const int sock_flags = fcntl(sock, F_GETFL, 0);

        if (sock_flags < 0) {
            CALL_ERR("1 fcntl");
            return -1;
        }

        if (fcntl(sock, F_SETFL, sock_flags | SO_REUSEADDR) < 0) {
            CALL_ERR("2 fcntl");
            return -1;
        }

        if (bind(sock, curr->ai_addr, curr->ai_addrlen) < 0) {
            CALL_ERR("bind");
            if (close(sock) < 0) {
                CALL_ERR("close");
            }
            sock = -1;
            continue;
        }

        break;
    }

    return sock;
}

static int make_listen_socket(const char *addr, const uint16_t port,
        const int backlog_size)
{
    struct addrinfo *list = make_addrinfo_list(addr, port);

    if (NULL == list) {
        return -1;
    }

    const int listen_sock = bind_socket(list);

    freeaddrinfo(list);

    if (listen_sock < 0) {
        return -1;
    }

    if (listen(listen_sock, backlog_size) == -1) {
        CALL_ERR("listen");
        if (close(listen_sock) < 0) {
            CALL_ERR("close");
        }
        return -1;
    }

    return listen_sock;
}

static int accept_connection(log_t *log, const int listen_sock)
{
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);
    const int sock = accept(listen_sock, (struct sockaddr *) &addr, &addr_size);

    if (sock < 0) {
        CALL_ERR("accept");
        return -1;
    }

    const int sock_flags = fcntl(sock, F_GETFL, 0);

    if (sock_flags < 0) {
        CALL_ERR("1 fcntl");
        return -1;
    }

    if (fcntl(sock, F_SETFL, sock_flags | O_NONBLOCK) < 0) {
        CALL_ERR("2 fcntl");
        return -1;
    }

    char hostname[100];

    if (inet_ntop(AF_INET, &addr.sin_addr, hostname, addr_size) == NULL) {
        CALL_ERR("inet_ntop");
    }

    log_write(log, "accept connection from %s:%d", hostname, addr.sin_port);

    return sock;
}

static worker_t *select_worker(worker_pool_t *pool)
{
    const size_t end = pool->current;
    size_t next = (end + 1) % pool->size;

    if (end != next) {
        while (end != next && worker_status(&pool->workers[next]) != WORKER_RUNNING) {
            next = (next + 1) % pool->size;
        }
    }

    if (worker_status(&pool->workers[next]) == WORKER_RUNNING) {
        pool->current = next;
        return &pool->workers[next];
    }

    pool->current = pool->size - 1;

    return NULL;
}

static int delegate_client(const int client_sock, worker_pool_t *workers)
{
    int result = 0;

    while (1) {
        worker_t* worker = select_worker(workers);

        if (NULL == worker) {
            result = -1;
            break;
        }

        if (worker_send_socket(worker, client_sock) == 0) {
            break;
        }
    }

    if (close(client_sock) < 0) {
        CALL_ERR("close");
    }

    return result;
}

static int reinit_bad_workers(worker_pool_t *pool)
{
    for (size_t i = 0; i < pool->size; ++i) {
        worker_t *worker = &pool->workers[i];

        if (WORKER_RUNNING == worker->__status) {
            continue;
        }

        worker_destroy(worker);

        if (worker_init(worker, pool->settings, pool->log) < 0) {
            return -1;
        }
    }

    return 0;
}

static int single_serve_listen_socket(const int listen_sock, worker_pool_t *workers)
{
    const int client_sock = accept_connection(workers->log, listen_sock);

    if (client_sock < 0) {
        return 1;
    }

    if (delegate_client(client_sock, workers) < 0) {
        return -1;
    }

    if (reinit_bad_workers(workers) < 0) {
        return -1;
    }

    return 0;
}

static int serve_listen_socket(const int listen_sock, worker_pool_t *workers)
{
    log_write(workers->log, "run server");

    while (0 == done) {
        if (single_serve_listen_socket(listen_sock, workers) < 0) {
            return -1;
        }
    }

    log_write(workers->log, "stop server");

    return 0;
}

int server_run(const settings_t *settings)
{
    if (settings->daemon) {
        const pid_t child = fork();

        if (child < 0) {
            CALL_ERR("fork");
            return -1;
        } else if (child > 0) {
            exit(0);
        } else if (daemon(0, 0) < 0) {
            CALL_ERR("daemon");
            return -1;
        }
    }

    log_t log;

    switch (log_init(&log, settings->log)) {
        case -1:
            return -1;
        case 0:
            break;
        case 1:
            return 0;
    }

    if (log_open(&log) < 0) {
        return -1;
    }

    const int listen_sock = make_listen_socket(settings->address, settings->port,
        settings->backlog_size);

    if (listen_sock < 0) {
        log_destroy(&log);
        return -1;
    }

    worker_pool_t workers;

    switch (init_worker_pool(&workers, settings->workers_count, settings, &log)) {
        case -1:
            if (close(listen_sock) < 0) {
                CALL_ERR("close");
            }
            log_destroy(&log);
            return -1;
        case 0:
            break;
        case 1:
            return 0;
    }

    if (set_server_signals_handle() < 0) {
        if (close(listen_sock) < 0) {
            CALL_ERR("close");
        }
        log_destroy(&log);
        free_worker_pool(&workers);
        return -1;
    }

    const int result = serve_listen_socket(listen_sock, &workers);

    free_worker_pool(&workers);
    log_destroy(&log);

    return result;
}
