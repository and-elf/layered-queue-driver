/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform abstraction layer for layered queue driver.
 * Allows building and testing without Zephyr.
 */

#ifndef LQ_PLATFORM_H_
#define LQ_PLATFORM_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform detection */
#if defined(__ZEPHYR__)
    #define LQ_PLATFORM_ZEPHYR 1
#else
    #define LQ_PLATFORM_NATIVE 1
#endif

/* ============================================================================
 * Time API
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 * @return Milliseconds since system start
 */
uint32_t lq_platform_uptime_get(void);

/**
 * @brief Sleep for specified milliseconds
 * @param ms Milliseconds to sleep
 */
void lq_platform_sleep_ms(uint32_t ms);

/* ============================================================================
 * Mutex API
 * ============================================================================ */

#ifdef LQ_PLATFORM_NATIVE
#include <pthread.h>
#ifndef __APPLE__
#include <semaphore.h>
#endif

struct lq_mutex {
    pthread_mutex_t mutex;
};

#ifdef __APPLE__
/* macOS: Use mutex+cond instead of deprecated sem_init */
struct lq_sem {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint32_t count;
    uint32_t max_count;
};
#else
/* Other POSIX platforms: Use standard semaphores */
struct lq_sem {
    sem_t sem;
    uint32_t max_count;
};
#endif

#elif defined(__ZEPHYR__)
#include <zephyr/kernel.h>

struct lq_mutex {
    struct k_mutex mutex;
};

struct lq_sem {
    struct k_sem sem;
    uint32_t max_count;
};

typedef struct {
    atomic_t value;
} lq_atomic_t;

#else
/* Other platforms will define these */
struct lq_mutex;
struct lq_sem;
#endif

typedef struct lq_mutex lq_mutex_t;
typedef struct lq_sem lq_sem_t;

/**
 * @brief Initialize a mutex
 * @param mutex Pointer to mutex structure
 * @return 0 on success, negative errno on failure
 */
int lq_mutex_init(lq_mutex_t *mutex);

/**
 * @brief Destroy a mutex
 * @param mutex Pointer to mutex structure
 */
void lq_mutex_destroy(lq_mutex_t *mutex);

/* ============================================================================
 * Semaphore API
 * ============================================================================ */

/**
 * @brief Initialize a semaphore
 * @param mutex Pointer to mutex structure
 * @return 0 on success, negative errno on failure
 */
int lq_mutex_lock(lq_mutex_t *mutex);

/**
 * @brief Unlock a mutex
 * @param mutex Pointer to mutex structure
 * @return 0 on success, negative errno on failure
 */
int lq_mutex_unlock(lq_mutex_t *mutex);

/**
 * @brief Destroy a mutex
 * @param mutex Pointer to mutex structure
 */
void lq_mutex_destroy(lq_mutex_t *mutex);

/**
 * @brief Lock a mutex (blocking)
 * @param sem Pointer to semaphore structure
 * @param initial_count Initial count
 * @param max_count Maximum count
 * @return 0 on success, negative errno on failure
 */
int lq_sem_init(lq_sem_t *sem, uint32_t initial_count, uint32_t max_count);

/**
 * @brief Take (decrement) a semaphore
 * @param sem Pointer to semaphore structure
 * @param timeout_ms Timeout in milliseconds (0=no wait, UINT32_MAX=forever)
 * @return 0 on success, -EAGAIN on timeout, negative errno on error
 */
int lq_sem_take(lq_sem_t *sem, uint32_t timeout_ms);

/**
#ifndef __ZEPHYR__
typedef struct {
    volatile int32_t value;
} lq_atomic_t;
#endifn success, negative errno on failure
 */
int lq_sem_give(lq_sem_t *sem);

/**
 * @brief Destroy a semaphore
 * @param sem Pointer to semaphore structure
 */
void lq_sem_destroy(lq_sem_t *sem);

/* ============================================================================
 * Atomic API
 * ============================================================================ */

typedef struct {
    volatile int32_t value;
} lq_atomic_t;

/**
 * @brief Atomically set a value
 * @param target Pointer to atomic variable
 * @param value Value to set
 */
void lq_atomic_set(lq_atomic_t *target, int32_t value);

/**
 * @brief Atomically get a value
 * @param target Pointer to atomic variable
 * @return Current value
 */
int32_t lq_atomic_get(const lq_atomic_t *target);

/**
 * @brief Atomically increment a value
 * @param target Pointer to atomic variable
 * @return Previous value
 */
int32_t lq_atomic_inc(lq_atomic_t *target);

/**
 * @brief Atomically decrement a value
 * @param target Pointer to atomic variable
 * @return Previous value
 */
int32_t lq_atomic_dec(lq_atomic_t *target);

/* ============================================================================
 * Memory API
 * ============================================================================ */

/**
 * @brief Allocate memory
 * @param size Size in bytes
 * @return Pointer to allocated memory, NULL on failure
 */
void *lq_malloc(size_t size);

/**
 * @brief Free memory
 * @param ptr Pointer to memory to free
 */
void lq_free(void *ptr);

/* ============================================================================
 * Logging API
 * ============================================================================ */

typedef enum {
    LQ_LOG_LEVEL_ERR = 0,
    LQ_LOG_LEVEL_WRN = 1,
    LQ_LOG_LEVEL_INF = 2,
    LQ_LOG_LEVEL_DBG = 3,
} lq_log_level_t;

/**
 * @brief Log a message
 * @param level Log level
 * @param module Module name
 * @param fmt Format string (printf-style)
 * @param ... Arguments
 */
void lq_log(lq_log_level_t level, const char *module, const char *fmt, ...);

/* Convenience macros */
#define LQ_LOG_ERR(module, fmt, ...) lq_log(LQ_LOG_LEVEL_ERR, module, fmt, ##__VA_ARGS__)
#define LQ_LOG_WRN(module, fmt, ...) lq_log(LQ_LOG_LEVEL_WRN, module, fmt, ##__VA_ARGS__)
#define LQ_LOG_INF(module, fmt, ...) lq_log(LQ_LOG_LEVEL_INF, module, fmt, ##__VA_ARGS__)
#define LQ_LOG_DBG(module, fmt, ...) lq_log(LQ_LOG_LEVEL_DBG, module, fmt, ##__VA_ARGS__)

/* ============================================================================
 * Error codes (POSIX-compatible)
 * ============================================================================ */

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef ENOMEM
#define ENOMEM 12
#endif

#ifndef EAGAIN
#define EAGAIN 11
#endif

#ifndef ENODATA
#define ENODATA 61
#endif

#ifndef ENOTSUP
#define ENOTSUP 95
#endif

#ifndef ENODEV
#define ENODEV 19
#endif

#ifdef __cplusplus
}
#endif

#endif /* LQ_PLATFORM_H_ */
