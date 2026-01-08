# Testing Socket Operations in C with Dependency Injection

## Overview

This document explains how we implemented dependency injection (DI) for socket operations to enable comprehensive testing of C code without requiring real sockets, demonstrated with `lq_hil.c`.

## The Challenge

Testing code that directly calls system functions like `socket()`, `connect()`, `send()`, and `recv()` is difficult because:
- Real sockets require OS resources and permissions
- Network operations are slow and unpredictable
- Hard to simulate error conditions
- Cannot run tests in parallel
- Requires complex test infrastructure

## The Solution: Platform Abstraction Layer

We wrapped all socket/system operations in a **function pointer struct** that can be swapped at runtime.

### 1. Define the Platform Operations Interface

```c
/* include/lq_hil_platform.h */
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

const struct lq_hil_platform_ops *lq_hil_get_platform_ops(void);
void lq_hil_set_platform_ops(const struct lq_hil_platform_ops *ops);
void lq_hil_reset_platform_ops(void);
```

### 2. Implement Default (Production) Operations

```c
/* src/drivers/lq_hil_platform.c */
static int default_socket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);  // Real system call
}

static int default_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect(sockfd, addr, addrlen);  // Real system call
}

// ... more wrappers ...

static const struct lq_hil_platform_ops default_ops = {
    .socket = default_socket,
    .connect = default_connect,
    // ... all other operations ...
};

static const struct lq_hil_platform_ops *current_ops = &default_ops;

const struct lq_hil_platform_ops *lq_hil_get_platform_ops(void) {
    return current_ops;
}

void lq_hil_set_platform_ops(const struct lq_hil_platform_ops *ops) {
    current_ops = ops ? ops : &default_ops;
}
```

### 3. Refactor Production Code to Use Ops

**Before:**
```c
int sock = socket(AF_UNIX, SOCK_STREAM, 0);
if (connect(sock, &addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
}
```

**After:**
```c
const struct lq_hil_platform_ops *ops = lq_hil_get_platform_ops();
int sock = ops->socket(AF_UNIX, SOCK_STREAM, 0);
if (ops->connect(sock, &addr, sizeof(addr)) < 0) {
    ops->close(sock);
    return -1;
}
```

### 4. Create Test Mocks

```c
/* tests/hil_platform_test.cpp */
static int mock_socket_fd = 100;

static int test_socket(int domain, int type, int protocol) {
    return mock_socket_fd++;  // Return fake FD
}

static int test_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return 0;  // Always succeed
}

static ssize_t test_send(int sockfd, const void *buf, size_t len, int flags) {
    return (ssize_t)len;  // Pretend we sent everything
}

static struct lq_hil_platform_ops test_ops = {
    .socket = test_socket,
    .connect = test_connect,
    .send = test_send,
    // ... all other operations ...
};
```

### 5. Write Tests

```cpp
class HILPlatformTest : public ::testing::Test {
protected:
    void SetUp() override {
        lq_hil_set_platform_ops(&test_ops);  // Inject mocks
    }
    
    void TearDown() override {
        lq_hil_reset_platform_ops();  // Restore defaults
    }
};

TEST_F(HILPlatformTest, InitTesterMode) {
    int ret = lq_hil_init(LQ_HIL_MODE_TESTER, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(lq_hil_is_active());
}

TEST_F(HILPlatformTest, TesterInjectADCSuccess) {
    lq_hil_init(LQ_HIL_MODE_TESTER, 0);
    int ret = lq_hil_tester_inject_adc(3, 0x123);
    EXPECT_EQ(ret, 0);
}
```

## Why Not GMock?

While GMock is powerful, we chose simple manual mocks for this project because:

1. **Simplicity**: Function pointers are a native C pattern
2. **No Dependencies**: Avoids gmock version compatibility issues
3. **Clarity**: Tests are easier to read and understand
4. **Flexibility**: Can easily control return values and simulate errors
5. **Performance**: No overhead from mock framework

## Alternative: Using GMock

If you prefer GMock, you can wrap the function pointers:

```cpp
class MockPlatformOps {
public:
    MOCK_METHOD(int, socket, (int, int, int));
    MOCK_METHOD(int, connect, (int, const struct sockaddr*, socklen_t));
    // ...
    static MockPlatformOps *instance;
};

extern "C" {
    static int mock_socket(int domain, int type, int protocol) {
        return MockPlatformOps::instance->socket(domain, type, protocol);
    }
    // ... more wrappers ...
}

TEST_F(Test, Example) {
    MockPlatformOps mock;
    MockPlatformOps::instance = &mock;
    
    EXPECT_CALL(mock, socket(_, _, _)).WillOnce(Return(10));
    EXPECT_CALL(mock, connect(10, _, _)).WillOnce(Return(0));
    
    // ... test code ...
}
```

## Benefits

✅ **Testable**: Can unit test socket code without real sockets  
✅ **Fast**: Tests run in microseconds instead of milliseconds  
✅ **Reliable**: No network flakiness or timing issues  
✅ **Error Simulation**: Easy to test error paths  
✅ **Isolated**: Tests don't interfere with each other  
✅ **Portable**: Works anywhere, no special setup needed  

## Results

- **Coverage Improvement**: 75.6% → 78.5% (+2.9%)
- **New Tests**: 13 HIL platform tests
- **Total Tests**: 332 tests passing
- **Test Speed**: All tests run in ~4 seconds

## Pattern Summary

This is the classic **Dependency Injection** pattern adapted for C:

1. Define interface (function pointer struct)
2. Provide default implementation (real system calls)
3. Allow runtime substitution (setter function)
4. Use interface in production code
5. Inject mocks in tests

This same pattern works for:
- File I/O (`open`, `read`, `write`)
- Time functions (`time`, `clock_gettime`)
- Hardware access (GPIO, SPI, I2C)
- Any external dependency

## Naming Convention

Use `_fn` suffix for function pointers that conflict with standard library names:
- `poll_fn` instead of `poll` (conflicts with `poll()`)
- `usleep_fn` instead of `usleep` (conflicts with `usleep()`)

This avoids compiler warnings and name collisions.
