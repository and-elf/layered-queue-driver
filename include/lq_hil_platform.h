/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * HIL Platform Abstraction Layer
 * 
 * Provides dependency injection for socket operations to enable testing.
 * Production code uses real sockets, tests use mocks.
 */

#ifndef LQ_HIL_PLATFORM_H
#define LQ_HIL_PLATFORM_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform operations interface */
struct lq_hil_platform_ops {
    /* Socket operations */
    int (*socket)(int domain, int type, int protocol);
    int (*bind)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    int (*listen)(int sockfd, int backlog);
    int (*connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    int (*accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    int (*close)(int fd);
    ssize_t (*send)(int sockfd, const void *buf, size_t len, int flags);
    ssize_t (*recv)(int sockfd, void *buf, size_t len, int flags);
    int (*poll_fn)(struct pollfd *fds, nfds_t nfds, int timeout);
    
    /* File operations */
    int (*fcntl)(int fd, int cmd, ...);
    int (*unlink)(const char *pathname);
    
    /* System operations */
    int (*usleep_fn)(useconds_t usec);
    const char *(*getenv)(const char *name);
    int (*getpid)(void);
};

/* Get current platform operations (default: real system calls) */
const struct lq_hil_platform_ops *lq_hil_get_platform_ops(void);

/* Set platform operations (for testing) */
void lq_hil_set_platform_ops(const struct lq_hil_platform_ops *ops);

/* Reset to default platform operations */
void lq_hil_reset_platform_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* LQ_HIL_PLATFORM_H */
