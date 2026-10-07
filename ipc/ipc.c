#include "ipc.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>

/*
 * write(2)/read(2) ne garantissent pas de transférer tout le buffer d'un coup
 * (surtout si sizeof(response_t) dépasse PIPE_BUF). On boucle jusqu'à complet.
 */
static int write_full(int fd, const void *buf, size_t n)
{
    const char *p = (const char *)buf;
    size_t left = n;

    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (w == 0)
            return -1;
        p += (size_t)w;
        left -= (size_t)w;
    }
    return 0;
}

static int read_full(int fd, void *buf, size_t n)
{
    char *p = (char *)buf;
    size_t left = n;

    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return 1; /* EOF */
        p += (size_t)r;
        left -= (size_t)r;
    }
    return 0;
}

int ipc_pipe_create(int fds[2])
{
    return pipe(fds);
}

int ipc_send_request(int fd, const request_t *req)
{
    return write_full(fd, req, sizeof(*req));
}

int ipc_recv_request(int fd, request_t *req)
{
    return read_full(fd, req, sizeof(*req));
}

int ipc_send_response(int fd, const response_t *resp)
{
    return write_full(fd, resp, sizeof(*resp));
}

int ipc_recv_response(int fd, response_t *resp)
{
    return read_full(fd, resp, sizeof(*resp));
}
