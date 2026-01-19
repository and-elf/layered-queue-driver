/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr platform implementation
 */

#include "lq_platform.h"
#include "lq_engine.h"

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* ============================================================================
 * Time implementation
 * ============================================================================ */

uint32_t lq_platform_uptime_get(void)
{
    return k_uptime_get_32();
}

uint64_t lq_platform_get_time_us(void)
{
    return k_uptime_get() * 1000ULL;  /* Convert ms to us */
}

void lq_platform_sleep_ms(uint32_t ms)
{
    k_sleep(K_MSEC(ms));
}

/* ============================================================================
 * Mutex implementation
 * ============================================================================ */

int lq_mutex_init(lq_mutex_t *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    return k_mutex_init(&mutex->mutex);
}

int lq_mutex_lock(lq_mutex_t *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    return k_mutex_lock(&mutex->mutex, K_FOREVER);
}

int lq_mutex_unlock(lq_mutex_t *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    return k_mutex_unlock(&mutex->mutex);
}

void lq_mutex_destroy(lq_mutex_t *mutex)
{
    /* Zephyr mutexes don't need explicit cleanup */
    (void)mutex;
}

/* ============================================================================
 * Semaphore implementation
 * ============================================================================ */

int lq_sem_init(lq_sem_t *sem, uint32_t initial_count, uint32_t max_count)
{
    if (!sem) {
        return -EINVAL;
    }
    
    sem->max_count = max_count;
    return k_sem_init(&sem->sem, initial_count, max_count);
}

int lq_sem_take(lq_sem_t *sem, uint32_t timeout_ms)
{
    if (!sem) {
        return -EINVAL;
    }
    
    k_timeout_t timeout;
    if (timeout_ms == 0) {
        timeout = K_NO_WAIT;
    } else if (timeout_ms == UINT32_MAX) {
        timeout = K_FOREVER;
    } else {
        timeout = K_MSEC(timeout_ms);
    }
    
    int ret = k_sem_take(&sem->sem, timeout);
    return ret == -EAGAIN ? -EAGAIN : ret;
}

int lq_sem_give(lq_sem_t *sem)
{
    if (!sem) {
        return -EINVAL;
    }
    
    k_sem_give(&sem->sem);
    return 0;
}

void lq_sem_destroy(lq_sem_t *sem)
{
    /* Zephyr semaphores don't need explicit cleanup */
    (void)sem;
}

/* ============================================================================
 * Atomic implementation
 * ============================================================================ */

void lq_atomic_set(lq_atomic_t *target, int32_t value)
{
    atomic_set(&target->value, value);
}

int32_t lq_atomic_get(const lq_atomic_t *target)
{
    return atomic_get((atomic_t *)&target->value);
}

int32_t lq_atomic_inc(lq_atomic_t *target)
{
    return atomic_inc(&target->value);
}

int32_t lq_atomic_dec(lq_atomic_t *target)
{
    return atomic_dec(&target->value);
}

/* ============================================================================
 * Memory implementation
 * ============================================================================ */

void *lq_malloc(size_t size)
{
    return k_malloc(size);
}

void lq_free(void *ptr)
{
    k_free(ptr);
}

/* ============================================================================
 * Logging implementation
 * ============================================================================ */

void lq_log(lq_log_level_t level, const char *module, const char *fmt, ...)
{
    const char *level_str[] = {"ERR", "WRN", "INF", "DBG"};
    
    printk("[%08u] <%s> %s: ", k_uptime_get_32(), level_str[level], module);
    
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
    
    printk("\n");
}

/* ============================================================================
 * Engine runner - main loop for Zephyr
 * ============================================================================ */

/* Forward declarations */
extern struct lq_engine g_lq_engine;
extern void lq_generated_dispatch_outputs(void);

int lq_engine_run(void)
{
    printk("Running layered-queue engine on Zephyr\n");
    
    /* Main engine loop */
    while (1) {
        uint64_t now = lq_platform_get_time_us();
        
        /* Run engine processing - updates signals and creates output events */
        lq_engine_step(&g_lq_engine, now, NULL, 0);
        
        /* Dispatch output events to hardware/protocol drivers */
        lq_generated_dispatch_outputs();
        
        /* Sleep for 10ms between cycles */
        lq_platform_sleep_ms(10);
    }
    
    return 0;
}

#endif /* __ZEPHYR__ */
