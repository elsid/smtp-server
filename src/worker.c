#include <arpa/inet.h>
#include <bsd/sys/tree.h>
#include <poll.h>
#include <sys/wait.h>

#include "protocol.h"
#include "signal_handle.h"
#include "worker.h"

static void do_nothing(int signum) {}

static int set_worker_signals_handle()
{
    return set_signals_handle(do_nothing);
}

static int recv_message(const int fd, void *data, const size_t size)
{
    char iov_data[] = {0};

    struct iovec iov = {
        .iov_base = iov_data,
        .iov_len = sizeof(iov_data)
    };

    char buffer[CMSG_SPACE(sizeof(int))];

    struct msghdr hdr = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = iov.iov_len,
        .msg_control = buffer,
        .msg_controllen = sizeof(buffer)
    };

    const ssize_t received = recvmsg(fd, &hdr, 0);

    if (received < 0) {
        CALL_ERR("recvmsg");
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&hdr);
    if (NULL == cmsg) {
        PRINT_STDERR("error: cmsg is null %s", "");
        return -1;
    }

    if (CMSG_LEN(size) != cmsg->cmsg_len) {
        PRINT_STDERR("error: bad data size %s", "");
        return -1;
    }

    memcpy(data, CMSG_DATA(cmsg), size);

    return 0;
}

static int recv_socket(int fd)
{
    int sock;

    if (recv_message(fd, &sock, sizeof(sock)) < 0) {
        return -1;
    }

    return sock;
}

typedef enum server_status {
    SERVER_RUNNING,
    SERVER_STOPPED
} server_status_t;

typedef struct client_node {
    int sock;
    context_t context;
    RB_ENTRY(client_node) entry;
} client_node_t;

static int client_node_cmp(client_node_t *first, client_node_t *second)
{
    return first->sock - second->sock;
}

RB_HEAD(client_tree, client_node);
RB_GENERATE(client_tree, client_node, entry, client_node_cmp)

typedef struct server {
    server_status_t status;
    int pipe_fd;
    struct client_tree clients;
    size_t clients_count;
    const settings_t *settings;
    log_t *log;
} server_t;

static void server_init(server_t *server, const int pipe_fd,
    const settings_t *settings, log_t *log)
{
    struct client_tree client_tree = RB_INITIALIZER(&client_tree);

    server->status = SERVER_RUNNING;
    server->pipe_fd = pipe_fd;
    server->clients = client_tree;
    server->clients_count = 0;
    server->settings = settings;
    server->log = log;
}

static void server_destroy(server_t *server)
{
    client_node_t *node, *temp;

    RB_FOREACH_SAFE(node, client_tree, &server->clients, temp) {
        RB_REMOVE(client_tree, &server->clients, node);

        if (close(node->sock) < 0) {
            CALL_ERR("close");
        }

        free(node);
    }

    if (server->pipe_fd < 0) {
        return;
    }

    if (close(server->pipe_fd) < 0) {
        CALL_ERR("close");
    }

    log_close(server->log);
}

static int add_client(server_t *server, const int sock)
{
    client_node_t *node = malloc(sizeof(client_node_t));

    if (NULL == node) {
        CALL_ERR_ARGS("malloc", "%lu", sizeof(client_node_t));
        return -1;
    }

    node->sock = sock;

    if (context_init(&node->context, sock, server->settings, server->log) < 0) {
        free(node);
        return -1;
    }

    RB_INSERT(client_tree, &server->clients, node);

    ++server->clients_count;

    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);

    if (getpeername(sock, (struct sockaddr *) &addr, &addr_size) < 0) {
        CALL_ERR("getpeername");
        return -1;
    }

    char hostname[100];

    if (inet_ntop(AF_INET, &addr.sin_addr, hostname, sizeof(hostname) - 1) < 0) {
        CALL_ERR("inet_ntop");
        return -1;
    }

    log_write(server->log, "[%s] process connection from %s:%d",
        node->context.uuid, hostname, addr.sin_port);

    return 0;
}

static void remove_client_node(server_t *server, client_node_t *node)
{
    if (NULL == node) {
        return;
    }

    context_destroy(&node->context);

    --server->clients_count;

    RB_REMOVE(client_tree, &server->clients, node);

    free(node);
}

static void remove_client(server_t *server, context_t *context)
{
    const int sock = context->socket;

    log_write(server->log, "[%s] close connection", context->uuid);

    client_node_t what = { .sock = sock };
    remove_client_node(server, RB_FIND(client_tree, &server->clients, &what));

    if (close(sock) < 0) {
        CALL_ERR("close");
    }
}

static int process_pipe(server_t *server, struct pollfd *pollfd)
{
    const short revents = pollfd->revents;

    pollfd->revents = 0;

    if ((revents & POLLERR) != 0) {
        int error = 0;
        socklen_t error_size = sizeof(error);

        if (getsockopt(pollfd->fd, SOL_SOCKET, SO_ERROR, &error, &error_size) < 0) {
            CALL_ERR("getsockopt");
            return -1;
        }

        log_write(server->log, "master socket error: %s", strerror(error));

        server->status = SERVER_STOPPED;
        server->pipe_fd = -1;

        return 0;
    }

    if ((revents & POLLHUP) != 0) {
        log_write(server->log, "master socket closed");
        server->status = SERVER_STOPPED;
        server->pipe_fd = -1;
        return 0;
    }

    if ((revents & POLLIN) != 0) {
        const int client_sock = recv_socket(pollfd->fd);

        if (client_sock < 0) {
            return -1;
        }

        return add_client(server, client_sock);
    }

    return 0;
}

static int serve_client_in(context_t *context)
{
    buffer_t *in_buf = &context->in_message;

    if (buffer_space(&context->in_message) == 0) {
        buffer_drop_read(&context->in_message);
    }

    while (1) {
        const size_t space = buffer_space(in_buf);

        if (0 == space) {
            break;
        }

        const ssize_t received = recv(context->socket, buffer_write_begin(in_buf),
            space, 0);

        if (received < 0) {
            CALL_ERR("recv");
            return -1;
        }

        if (0 == received) {
            break;
        }

        buffer_shift_write(in_buf, received);

        if (received <= space) {
            break;
        }
    }

    return 0;
}

static int serve_client_out(context_t *context)
{
    while (!buffer_tailq_empty(&context->out_message_queue)) {
        buffer_t *out_buf = buffer_tailq_front(&context->out_message_queue);

        if (buffer_left(out_buf) > 0) {
            const ssize_t sent = send(context->socket, buffer_read_begin(out_buf),
                buffer_left(out_buf), 0);

            if (sent < 0) {
                CALL_ERR("send");
                return -1;
            }

            buffer_shift_read(out_buf, sent);
        }

        if (buffer_left(out_buf) == 0) {
            buffer_tailq_pop_front(&context->out_message_queue);
        }
    }

    return 0;
}

static context_t *find_client_context(server_t *server, const int sock)
{
    client_node_t what = { .sock = sock };
    client_node_t *node = RB_FIND(client_tree, &server->clients, &what);

    if (NULL == node) {
        PRINT_STDERR("error find client: %d", sock);
        return NULL;
    }

    return &node->context;
}

static int serve_client(server_t *server, struct pollfd *pollfd)
{
    const int client_sock = pollfd->fd;
    context_t *context = find_client_context(server, client_sock);

    if ((pollfd->revents & POLLERR) != 0) {
        int error = 0;
        socklen_t error_size = sizeof(error);
        getsockopt(pollfd->fd, SOL_SOCKET, SO_ERROR, &error, &error_size);
        log_write(context->log, "[%s] socket error: %s", context->uuid,
            strerror(error));
        remove_client(server, context);
        return 0;
    }

    if ((pollfd->revents & POLLHUP) != 0) {
        log_write(context->log, "[%s] remote socket closed", context->uuid);
        remove_client(server, context);
        return 0;
    }

    if (NULL == context) {
        PRINT_STDERR("error find client context: %d", client_sock);
        return -1;
    }

    if ((pollfd->revents & POLLIN) != 0) {
        if (serve_client_in(context) < 0) {
            log_write(context->log, "[%s] receive command error: %s",
                context->uuid, strerror(errno));
        }
    }

    if (process_client(context) < 0) {
        log_write(context->log, "[%s] process client error: %s",
            context->uuid, strerror(errno));
    }

    if ((pollfd->revents & POLLOUT) != 0) {
        if (serve_client_out(context) < 0) {
            log_write(context->log, "[%s] send command error: %s",
                context->uuid, strerror(errno));
        }
    }

    if (SMTP_SERVER_ST_DONE == context->state
            || SMTP_SERVER_ST_INVALID == context->state) {
        if (shutdown(context->socket, SHUT_RD) < 0) {
            CALL_ERR("shutdown");
        }
        if (buffer_tailq_empty(&context->out_message_queue)) {
            if (shutdown(context->socket, SHUT_RDWR) < 0) {
                CALL_ERR("shutdown");
            }
        }
    }

    return 0;
}

static int serve_socket(server_t *server, struct pollfd *pollfd)
{
    if (pollfd->fd == server->pipe_fd) {
        return process_pipe(server, pollfd);
    } else {
        return serve_client(server, pollfd);
    }
}

static struct pollfd *alloc_pollfds(server_t *server, size_t *count)
{
    size_t pollfds_count = (server->pipe_fd < 0 ? 0 : 1) + server->clients_count;

    if (NULL != count) {
        *count = pollfds_count;
    }

    if (0 == pollfds_count) {
        return NULL;
    }

    struct pollfd *pollfds = calloc(pollfds_count, sizeof(struct pollfd));

    if (NULL == pollfds) {
        return NULL;
    }

    client_node_t *node, *temp;
    size_t pollfds_index = 0;

    RB_FOREACH_SAFE(node, client_tree, &server->clients, temp) {
        pollfds[pollfds_index].fd = node->sock;
        pollfds[pollfds_index].events = POLLIN | POLLOUT | POLLERR| POLLHUP;
        pollfds[pollfds_index].revents = 0;
        ++pollfds_index;
    }

    if (server->pipe_fd >= 0) {
        pollfds[pollfds_count - 1].fd = server->pipe_fd;
        pollfds[pollfds_count - 1].events = POLLIN | POLLERR | POLLHUP;
        pollfds[pollfds_count - 1].revents = 0;
    }

    return pollfds;
}

static int single_serve(server_t *server)
{
    size_t pollfds_count;
    struct pollfd *pollfds = alloc_pollfds(server, &pollfds_count);

    if (NULL == pollfds) {
        return 0;
    }

    const int poll_result = poll(pollfds, pollfds_count, 1000);
    int result = 0;

    if (poll_result > 0) {
        for (size_t i = 0; 0 == result && i < pollfds_count; ++i) {
            if (serve_socket(server, &pollfds[i]) < 0) {
                result = -1;
            }
        }
    } else {
        result = poll_result;
    }

    free(pollfds);

    return result;
}

static int serve(server_t *server)
{
    log_write(server->log, "run worker");

    while (SERVER_RUNNING == server->status) {
        if (single_serve(server) < 0) {
            log_write(server->log, "abort worker");
            return -1;
        }
    }

    log_write(server->log, "stop worker");

    return 0;
}

static int worker_run(const int pipe_fd, const settings_t *settings, log_t *log)
{
    if (set_worker_signals_handle() < 0) {
        return -1;
    }

    server_t server;

    server_init(&server, pipe_fd, settings, log);

    const int result = serve(&server);

    server_destroy(&server);

    return result;
}

static int send_message(const int fd, const void *data, const size_t size)
{
    char iov_data[1] = {0};

    struct iovec iov = {
        .iov_base = iov_data,
        .iov_len = sizeof(iov_data)
    };

    char buffer[CMSG_SPACE(sizeof(int))];

    struct msghdr hdr = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = iov.iov_len,
        .msg_control = buffer,
        .msg_controllen = sizeof(buffer)
    };

    struct cmsghdr *msg = CMSG_FIRSTHDR(&hdr);

    msg->cmsg_level = SOL_SOCKET;
    msg->cmsg_type = SCM_RIGHTS;
    msg->cmsg_len = CMSG_LEN(size);

    memcpy(CMSG_DATA(msg), data, size);

    hdr.msg_controllen = msg->cmsg_len;

    ssize_t written;

    do {
        written = sendmsg(fd, &hdr, MSG_NOSIGNAL);
    } while (written < 0 && EINTR == errno);

    if (written < 0) {
        CALL_ERR("sendmsg");
        return -1;
    }

    return 0;
}

int worker_init(worker_t *worker, const settings_t *settings, log_t *log)
{
    int fd[2];

    if (socketpair(PF_UNIX, SOCK_DGRAM, 0, fd) < 0) {
        CALL_ERR("pipe");
        return -1;
    }

    const pid_t worker_pid = fork();

    if (worker_pid < 0) {
        CALL_ERR("fork");
        return -1;
    } else if (0 == worker_pid) {
        if (close(fd[1]) < 0) {
            CALL_ERR("close");
        }

        exit(worker_run(fd[0], settings, log));
    }

    if (close(fd[0]) < 0) {
        CALL_ERR("close");
    }

    worker->__pid = worker_pid;
    worker->__sock = fd[1];
    worker->__status = WORKER_RUNNING;

    return 0;
}

void worker_destroy(worker_t *worker)
{
    if (close(worker->__sock) < 0) {
        CALL_ERR_ARGS("close", "%d", worker->__sock);
    }

    if (waitpid(worker->__pid, NULL, 0) < 0) {
        CALL_ERR_ARGS("waitpid", "%d", worker->__pid);
    }
}

int worker_send_socket(worker_t *worker, const int sock)
{
    if (send_message(worker->__sock, &sock, sizeof(sock)) < 0) {
        worker->__status = WORKER_ERROR;
        return -1;
    }

    return 0;
}

worker_status_t worker_status(const worker_t *worker)
{
    return worker->__status;
}
