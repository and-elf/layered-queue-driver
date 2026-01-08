/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Unit tests for HIL (Hardware-in-the-Loop) with platform abstraction
 */

#include <gtest/gtest.h>
extern "C" {
#include "lq_hil.h"
#include "lq_hil_platform.h"
#include <errno.h>
#include <string.h>
}

/* Simple manual mock for testing */
static int mock_socket_fd = 100;
static int mock_connect_success = 1;
static int mock_bind_success = 1;

static int test_socket(int domain, int type, int protocol) {
    (void)domain; (void)type; (void)protocol;
    return mock_socket_fd++;
}

static int test_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return mock_bind_success ? 0 : -1;
}

static int test_listen(int sockfd, int backlog) {
    (void)sockfd; (void)backlog;
    return 0;
}

static int test_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return mock_connect_success ? 0 : -1;
}

static int test_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return 200;
}

static int test_close(int fd) {
    (void)fd;
    return 0;
}

static ssize_t test_send(int sockfd, const void *buf, size_t len, int flags) {
    (void)sockfd; (void)buf; (void)flags;
    return (ssize_t)len;
}

static ssize_t test_recv(int sockfd, void *buf, size_t len, int flags) {
    (void)sockfd; (void)buf; (void)flags;
    return (ssize_t)len;
}

static int test_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    (void)fds; (void)nfds; (void)timeout;
    return 1;
}

static int test_fcntl(int fd, int cmd, ...) {
    (void)fd; (void)cmd;
    return 0;
}

static int test_unlink(const char *pathname) {
    (void)pathname;
    return 0;
}

static int test_usleep(useconds_t usec) {
    (void)usec;
    return 0;
}

static int test_getpid(void) {
    return 12345;
}

static struct lq_hil_platform_ops test_ops = {
    .socket = test_socket,
    .bind = test_bind,
    .listen = test_listen,
    .connect = test_connect,
    .accept = test_accept,
    .close = test_close,
    .send = test_send,
    .recv = test_recv,
    .poll_fn = test_poll,
    .fcntl = test_fcntl,
    .unlink = test_unlink,
    .usleep_fn = test_usleep,
    .getpid = test_getpid,
};

class HILPlatformTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_socket_fd = 100;
        mock_connect_success = 1;
        mock_bind_success = 1;
        lq_hil_set_platform_ops(&test_ops);
    }
    
    void TearDown() override {
        lq_hil_cleanup();
        lq_hil_reset_platform_ops();
    }
};

TEST_F(HILPlatformTest, InitDisabledMode)
{
    int ret = lq_hil_init(LQ_HIL_MODE_DISABLED, nullptr, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(lq_hil_is_active());
}

TEST_F(HILPlatformTest, InitSUTMode)
{
    int ret = lq_hil_init(LQ_HIL_MODE_SUT, nullptr, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(lq_hil_is_active());
}

TEST_F(HILPlatformTest, InitTesterMode)
{
    int ret = lq_hil_init(LQ_HIL_MODE_TESTER, nullptr, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(lq_hil_is_active());
}

TEST_F(HILPlatformTest, InitAlreadyInitialized)
{
    lq_hil_init(LQ_HIL_MODE_DISABLED, nullptr, 0);
    int ret = lq_hil_init(LQ_HIL_MODE_DISABLED, nullptr, 0);
    EXPECT_EQ(ret, -EALREADY);
}

TEST_F(HILPlatformTest, CleanupSUTMode)
{
    lq_hil_init(LQ_HIL_MODE_SUT, nullptr, 0);
    lq_hil_cleanup();
    EXPECT_FALSE(lq_hil_is_active());
}

TEST_F(HILPlatformTest, CleanupTesterMode)
{
    lq_hil_init(LQ_HIL_MODE_TESTER, nullptr, 0);
    lq_hil_cleanup();
    EXPECT_FALSE(lq_hil_is_active());
}

TEST_F(HILPlatformTest, TesterInjectADCSuccess)
{
    lq_hil_init(LQ_HIL_MODE_TESTER, nullptr, 0);
    
    int ret = lq_hil_tester_inject_adc(3, 0x123);
    EXPECT_EQ(ret, 0);
}

TEST_F(HILPlatformTest, TesterInjectADCWrongMode)
{
    lq_hil_init(LQ_HIL_MODE_DISABLED, nullptr, 0);
    
    int ret = lq_hil_tester_inject_adc(3, 0x123);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(HILPlatformTest, TesterInjectCANSuccess)
{
    lq_hil_init(LQ_HIL_MODE_TESTER, nullptr, 0);
    
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    int ret = lq_hil_tester_inject_can(0x123, false, data, 8);
    EXPECT_EQ(ret, 0);
}

TEST_F(HILPlatformTest, SUTRecvADCSuccess)
{
    lq_hil_init(LQ_HIL_MODE_SUT, nullptr, 0);
    
    struct lq_hil_adc_msg msg;
    int ret = lq_hil_sut_recv_adc(&msg, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(HILPlatformTest, SUTSendGPIOSuccess)
{
    lq_hil_init(LQ_HIL_MODE_SUT, nullptr, 0);
    
    int ret = lq_hil_sut_send_gpio(5, 1);
    EXPECT_EQ(ret, 0);
}

TEST_F(HILPlatformTest, TimestampMonotonic)
{
    uint64_t t1 = lq_hil_get_timestamp_us();
    usleep(10);  // Small delay to ensure time difference
    uint64_t t2 = lq_hil_get_timestamp_us();
    
    EXPECT_GT(t2, t1);
}

TEST_F(HILPlatformTest, PlatformOpsGetSet)
{
    const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
    EXPECT_NE(ops, nullptr);
    EXPECT_NE(ops->socket, nullptr);
    EXPECT_NE(ops->bind, nullptr);
}
