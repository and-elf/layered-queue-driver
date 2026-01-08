/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hardware-in-the-Loop (HIL) Implementation (Linux/Native)
 */

#include "lq_hil.h"
#include "lq_hil_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <poll.h>

/* HIL state */
static struct {
    enum lq_hil_mode mode;
    int pid;
    
    /* Socket file descriptors */
    int sock_adc;
    int sock_spi;
    int sock_can;
    int sock_gpio;
    int sock_sync;
    
    /* For SUT: listening sockets */
    int listen_adc;
    int listen_spi;
    int listen_can;
    int listen_gpio;
    int listen_sync;
    
    bool initialized;
} hil_state = {
    .mode = LQ_HIL_MODE_DISABLED,
    .initialized = false,
    .sock_adc = -1,
    .sock_spi = -1,
    .sock_can = -1,
    .sock_gpio = -1,
    .sock_sync = -1,
    .listen_adc = -1,
    .listen_spi = -1,
    .listen_can = -1,
    .listen_gpio = -1,
    .listen_sync = -1,
};

/* Helper: Create socket path */
static void make_socket_path(char *buf, size_t size, const char *fmt, int pid)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    snprintf(buf, size, fmt, pid);
#pragma GCC diagnostic pop
}

/* Helper: Create Unix domain socket */
static int create_unix_socket(const struct lq_hil_platform_ops *ops, const char *path, bool is_server)
{
    
    int sock = ops->socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return -errno;
    }
    
    /* Set non-blocking mode (portable approach for macOS/BSD compatibility) */
    int flags = ops->fcntl(sock, F_GETFL, 0);
    if (flags < 0 || ops->fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        ops->close(sock);
        return -errno;
    }
    
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    
    if (is_server) {
        /* Remove existing socket file */
        ops->unlink(path);
        
        if (ops->bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ops->close(sock);
            return -errno;
        }
        
        if (ops->listen(sock, 1) < 0) {
            ops->close(sock);
            ops->unlink(path);
            return -errno;
        }
    } else {
        /* Client: connect with retry */
        int retries = 50;  /* 5 seconds */
        while (retries-- > 0) {
            int ret = ops->connect(sock, (struct sockaddr *)&addr, sizeof(addr));
            if (ret == 0 || (ret < 0 && errno == EISCONN)) {
                return sock;
            }
            if (ret < 0 && errno != EINPROGRESS && errno != EALREADY) {
                break;  /* Unrecoverable error */
            }
            ops->usleep_fn(100000);  /* 100ms */
        }
        ops->close(sock);
        return -ETIMEDOUT;
    }
    
    return sock;
}

/* Helper: Accept connection on listening socket */
static int accept_connection(const struct lq_hil_platform_ops *ops, int listen_sock, int timeout_ms)
{
    
    struct pollfd pfd = {
        .fd = listen_sock,
        .events = POLLIN,
    };
    
    int ret = ops->poll_fn(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return -ETIMEDOUT;
    }
    
    int client = ops->accept(listen_sock, NULL, NULL);
    if (client < 0) {
        return -errno;
    }
    
    return client;
}

int lq_hil_init(enum lq_hil_mode mode, const char *mode_str, int pid)
{
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    if (hil_state.initialized) {
        return -EALREADY;
    }
    
    /* Override mode from string if provided */
    if (mode_str != NULL) {
        if (strcmp(mode_str, "sut") == 0) {
            mode = LQ_HIL_MODE_SUT;
        } else if (strcmp(mode_str, "tester") == 0) {
            mode = LQ_HIL_MODE_TESTER;
        } else if (strcmp(mode_str, "disabled") == 0) {
            mode = LQ_HIL_MODE_DISABLED;
        }
    }
    
    hil_state.mode = mode;
    hil_state.pid = (pid == 0) ? ops->getpid() : pid;
    
    if (mode == LQ_HIL_MODE_DISABLED) {
        hil_state.initialized = true;
        return 0;
    }
    
    char path[256];
    
    if (mode == LQ_HIL_MODE_SUT) {
        /* Create listening sockets */
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_ADC, hil_state.pid);
        hil_state.listen_adc = create_unix_socket(ops, path, true);
        
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_SPI, hil_state.pid);
        hil_state.listen_spi = create_unix_socket(ops, path, true);
        
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_CAN, hil_state.pid);
        hil_state.listen_can = create_unix_socket(ops, path, true);
        
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_GPIO, hil_state.pid);
        hil_state.listen_gpio = create_unix_socket(ops, path, true);
        
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_SYNC, hil_state.pid);
        hil_state.listen_sync = create_unix_socket(ops, path, true);
        
        printf("[HIL-SUT] Listening on sockets for PID %d\n", hil_state.pid);
        
    } else if (mode == LQ_HIL_MODE_TESTER) {
        /* Connect to SUT sockets */
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_ADC, hil_state.pid);
        hil_state.sock_adc = create_unix_socket(ops, path, false);
        
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_SPI, hil_state.pid);
        hil_state.sock_spi = create_unix_socket(ops, path, false);
        
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_CAN, hil_state.pid);
        hil_state.sock_can = create_unix_socket(ops, path, false);
        
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_GPIO, hil_state.pid);
        hil_state.sock_gpio = create_unix_socket(ops, path, false);
        
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_SYNC, hil_state.pid);
        hil_state.sock_sync = create_unix_socket(ops, path, false);
        
        printf("[HIL-Tester] Connected to SUT PID %d\n", hil_state.pid);
    }
    
    hil_state.initialized = true;
    return 0;
}

void lq_hil_cleanup(void)
{
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    if (!hil_state.initialized) {
        return;
    }
    
    if (hil_state.mode == LQ_HIL_MODE_SUT) {
        if (hil_state.listen_adc >= 0) ops->close(hil_state.listen_adc);
        if (hil_state.listen_spi >= 0) ops->close(hil_state.listen_spi);
        if (hil_state.listen_can >= 0) ops->close(hil_state.listen_can);
        if (hil_state.listen_gpio >= 0) ops->close(hil_state.listen_gpio);
        if (hil_state.listen_sync >= 0) ops->close(hil_state.listen_sync);
        
        /* Remove socket files */
        char path[256];
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_ADC, hil_state.pid);
        ops->unlink(path);
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_SPI, hil_state.pid);
        ops->unlink(path);
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_CAN, hil_state.pid);
        ops->unlink(path);
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_GPIO, hil_state.pid);
        ops->unlink(path);
        make_socket_path(path, sizeof(path), LQ_HIL_SOCKET_SYNC, hil_state.pid);
        ops->unlink(path);
        
    } else if (hil_state.mode == LQ_HIL_MODE_TESTER) {
        if (hil_state.sock_adc >= 0) ops->close(hil_state.sock_adc);
        if (hil_state.sock_spi >= 0) ops->close(hil_state.sock_spi);
        if (hil_state.sock_can >= 0) ops->close(hil_state.sock_can);
        if (hil_state.sock_gpio >= 0) ops->close(hil_state.sock_gpio);
        if (hil_state.sock_sync >= 0) ops->close(hil_state.sock_sync);
    }
    
    memset(&hil_state, 0, sizeof(hil_state));
}

bool lq_hil_is_active(void)
{
    return hil_state.initialized && (hil_state.mode != LQ_HIL_MODE_DISABLED);
}

uint64_t lq_hil_get_timestamp_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000U + (uint64_t)tv.tv_usec;
}

/* ============================================================================
 * SUT (System Under Test) Functions
 * ========================================================================== */

int lq_hil_sut_recv_adc(struct lq_hil_adc_msg *msg, int timeout_ms)
{
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    if (hil_state.mode != LQ_HIL_MODE_SUT) {
        return -EINVAL;
    }
    
    /* Accept connection if not yet connected */
    if (hil_state.sock_adc < 0) {
        hil_state.sock_adc = accept_connection(ops, hil_state.listen_adc, timeout_ms);
        if (hil_state.sock_adc < 0) {
            return hil_state.sock_adc;
        }
    }
    
    /* Receive message */
    struct pollfd pfd = {
        .fd = hil_state.sock_adc,
        .events = POLLIN,
    };
    
    int ret = ops->poll_fn(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return -EAGAIN;
    }
    
    ssize_t n = ops->recv(hil_state.sock_adc, msg, sizeof(*msg), 0);
    if (n != sizeof(*msg)) {
        return -EIO;
    }
    
    return 0;
}

int lq_hil_sut_recv_can(struct lq_hil_can_msg *msg, int timeout_ms)
{
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    if (hil_state.mode != LQ_HIL_MODE_SUT) {
        return -EINVAL;
    }
    
    if (hil_state.sock_can < 0) {
        hil_state.sock_can = accept_connection(ops, hil_state.listen_can, timeout_ms);
        if (hil_state.sock_can < 0) {
            return hil_state.sock_can;
        }
    }
    
    struct pollfd pfd = {
        .fd = hil_state.sock_can,
        .events = POLLIN,
    };
    
    int ret = ops->poll_fn(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return -EAGAIN;
    }
    
    ssize_t n = ops->recv(hil_state.sock_can, msg, sizeof(*msg), 0);
    if (n != sizeof(*msg)) {
        return -EIO;
    }
    
    return 0;
}

int lq_hil_sut_send_gpio(uint8_t pin, uint8_t state)
{
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    if (hil_state.mode != LQ_HIL_MODE_SUT) {
        return -EINVAL;
    }
    
    if (hil_state.sock_gpio < 0) {
        hil_state.sock_gpio = accept_connection(ops, hil_state.listen_gpio, 100);
        if (hil_state.sock_gpio < 0) {
            return 0;  /* No tester connected - OK */
        }
    }
    
    struct lq_hil_gpio_msg msg = {
        .hdr = {
            .type = LQ_HIL_MSG_GPIO,
            .timestamp_us = lq_hil_get_timestamp_us(),
        },
        .pin = pin,
        .state = state,
    };
    
    ssize_t n = ops->send(hil_state.sock_gpio, &msg, sizeof(msg), MSG_NOSIGNAL);
    if (n != sizeof(msg)) {
        return -EIO;
    }
    
    return 0;
}

int lq_hil_sut_send_can(const struct lq_hil_can_msg *msg)
{
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    if (hil_state.mode != LQ_HIL_MODE_SUT) {
        return -EINVAL;
    }
    
    /* Accept connection on first send if not yet connected */
    if (hil_state.sock_can < 0) {
        hil_state.sock_can = accept_connection(ops, hil_state.listen_can, 100);
        if (hil_state.sock_can < 0) {
            return 0;  /* No tester connected - OK */
        }
    }
    
    ssize_t n = ops->send(hil_state.sock_can, msg, sizeof(*msg), MSG_NOSIGNAL);
    if (n != sizeof(*msg)) {
        return -EIO;
    }
    
    return 0;
}

/* ============================================================================
 * Tester Functions
 * ========================================================================== */

int lq_hil_tester_inject_adc(uint8_t channel, uint32_t value)
{
    if (hil_state.mode != LQ_HIL_MODE_TESTER) {
        return -EINVAL;
    }
    
    struct lq_hil_adc_msg msg = {
        .hdr = {
            .type = LQ_HIL_MSG_ADC,
            .channel = channel,
            .timestamp_us = lq_hil_get_timestamp_us(),
        },
        .value = value,
    };
    
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    ssize_t n = ops->send(hil_state.sock_adc, &msg, sizeof(msg), 0);
    if (n != sizeof(msg)) {
        return -EIO;
    }
    
    return 0;
}

int lq_hil_tester_inject_can(uint32_t can_id, bool is_extended,
                              const uint8_t *data, uint8_t dlc)
{
    if (hil_state.mode != LQ_HIL_MODE_TESTER) {
        return -EINVAL;
    }
    
    struct lq_hil_can_msg msg = {
        .hdr = {
            .type = LQ_HIL_MSG_CAN,
            .timestamp_us = lq_hil_get_timestamp_us(),
        },
        .can_id = can_id,
        .is_extended = is_extended ? 1 : 0,
        .dlc = dlc,
    };
    
    if (dlc > 8) {
        dlc = 8;
    }
    memcpy(msg.data, data, dlc);
    
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    ssize_t n = ops->send(hil_state.sock_can, &msg, sizeof(msg), 0);
    if (n != sizeof(msg)) {
        return -EIO;
    }
    
    return 0;
}

int lq_hil_tester_wait_gpio(uint8_t pin, uint8_t expected_state, int timeout_ms)
{
    if (hil_state.mode != LQ_HIL_MODE_TESTER) {
        return -EINVAL;
    }
    
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    struct pollfd pfd = {
        .fd = hil_state.sock_gpio,
        .events = POLLIN,
    };
    
    int ret = ops->poll_fn(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return -ETIMEDOUT;
    }
    
    struct lq_hil_gpio_msg msg;
    ssize_t n = ops->recv(hil_state.sock_gpio, &msg, sizeof(msg), 0);
    if (n != sizeof(msg)) {
        return -EIO;
    }
    
    if (msg.pin != pin || msg.state != expected_state) {
        return -EINVAL;
    }
    
    return 0;
}

int lq_hil_tester_wait_can(struct lq_hil_can_msg *msg, int timeout_ms)
{
    if (hil_state.mode != LQ_HIL_MODE_TESTER) {
        return -EINVAL;
    }
    
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    
    struct pollfd pfd = {
        .fd = hil_state.sock_can,
        .events = POLLIN,
    };
    
    int ret = ops->poll_fn(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return -ETIMEDOUT;
    }
    
    ssize_t n = ops->recv(hil_state.sock_can, msg, sizeof(*msg), 0);
    if (n != sizeof(*msg)) {
        return -EIO;
    }
    
    return 0;
}
