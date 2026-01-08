/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * HIL Platform Implementation (Real System Calls)
 */

#include "lq_hil_platform.h"
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdlib.h>

/* Default platform operations using real system calls */
static int default_socket(int domain, int type, int protocol)
{
    return socket(domain, type, protocol);
}

static int default_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

static int default_listen(int sockfd, int backlog)
{
    return listen(sockfd, backlog);
}

static int default_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return connect(sockfd, addr, addrlen);
}

static int default_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return accept(sockfd, addr, addrlen);
}

static int default_close(int fd)
{
    return close(fd);
}

static ssize_t default_send(int sockfd, const void *buf, size_t len, int flags)
{
    return send(sockfd, buf, len, flags);
}

static ssize_t default_recv(int sockfd, void *buf, size_t len, int flags)
{
    return recv(sockfd, buf, len, flags);
}

static int default_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    return poll(fds, nfds, timeout);
}

static int default_fcntl(int fd, int cmd, ...)
{
    va_list args;
    va_start(args, cmd);
    int arg = va_arg(args, int);
    va_end(args);
    return fcntl(fd, cmd, arg);
}

static int default_unlink(const char *pathname)
{
    return unlink(pathname);
}

static int default_usleep(useconds_t usec)
{
    return usleep(usec);
}

static int default_getpid(void)
{
    return getpid();
}

/* Default operations */
static const struct lq_hil_platform_ops default_ops = {
    .socket = default_socket,
    .bind = default_bind,
    .listen = default_listen,
    .connect = default_connect,
    .accept = default_accept,
    .close = default_close,
    .send = default_send,
    .recv = default_recv,
    .poll_fn = default_poll,
    .fcntl = default_fcntl,
    .unlink = default_unlink,
    .usleep_fn = default_usleep,
    .getpid = default_getpid,
};

/* Current platform operations pointer */
static const struct lq_hil_platform_ops *current_ops = &default_ops;

const struct lq_hil_platform_ops *lq_hil_get_platform_ops(void)
{
    return current_ops;
}

void lq_hil_set_platform_ops(const struct lq_hil_platform_ops *ops)
{
    current_ops = ops ? ops : &default_ops;
}

void lq_hil_reset_platform_ops(void)
{
    current_ops = &default_ops;
}
