/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Native (POSIX) platform implementation
 */

#include "lq_platform.h"
#include "lq_hil.h"

#ifdef LQ_PLATFORM_NATIVE

#include <pthread.h>
#ifndef __APPLE__
#include <semaphore.h>
#endif
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

/* Static start time for uptime calculation */
static struct timeval start_time;
static pthread_once_t time_init_once = PTHREAD_ONCE_INIT;

static void init_start_time(void)
{
    gettimeofday(&start_time, NULL);
}

/* ============================================================================
 * Time implementation
 * ============================================================================ */

uint32_t lq_platform_uptime_get(void)
{
    pthread_once(&time_init_once, init_start_time);
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    uint32_t seconds = (uint32_t)(now.tv_sec - start_time.tv_sec);
    uint32_t useconds = (uint32_t)(now.tv_usec - start_time.tv_usec);
    
    return seconds * 1000 + useconds / 1000;
}

uint64_t lq_platform_get_time_us(void)
{
    pthread_once(&time_init_once, init_start_time);
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    uint64_t seconds = (uint64_t)(now.tv_sec - start_time.tv_sec);
    uint64_t useconds = (uint64_t)(now.tv_usec - start_time.tv_usec);
    
    return seconds * 1000000ULL + useconds;
}

void lq_platform_sleep_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

/* ============================================================================
 * Mutex implementation
 * ============================================================================ */

int lq_mutex_init(lq_mutex_t *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    int ret = pthread_mutex_init(&mutex->mutex, NULL);
    return ret == 0 ? 0 : -ret;
}

int lq_mutex_lock(lq_mutex_t *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    int ret = pthread_mutex_lock(&mutex->mutex);
    return ret == 0 ? 0 : -ret;
}

int lq_mutex_unlock(lq_mutex_t *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    int ret = pthread_mutex_unlock(&mutex->mutex);
    return ret == 0 ? 0 : -ret;
}

void lq_mutex_destroy(lq_mutex_t *mutex)
{
    if (mutex) {
        pthread_mutex_destroy(&mutex->mutex);
    }
}

/* ============================================================================
 * Semaphore implementation
 * ============================================================================ */

#ifdef __APPLE__
/* macOS: Use mutex+cond instead of deprecated sem_init */

int lq_sem_init(lq_sem_t *sem, uint32_t initial_count, uint32_t max_count)
{
    if (!sem) {
        return -EINVAL;
    }
    
    sem->count = initial_count;
    sem->max_count = max_count;
    
    int ret = pthread_mutex_init(&sem->mutex, NULL);
    if (ret != 0) {
        return -ret;
    }
    
    ret = pthread_cond_init(&sem->cond, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&sem->mutex);
        return -ret;
    }
    
    return 0;
}

#else
/* Other POSIX platforms: Use standard semaphores */

int lq_sem_init(lq_sem_t *sem, uint32_t initial_count, uint32_t max_count)
{
    if (!sem) {
        return -EINVAL;
    }
    
    sem->max_count = max_count;
    int ret = sem_init(&sem->sem, 0, initial_count);
    return ret == 0 ? 0 : -errno;
}

#endif

#ifdef __APPLE__
/* macOS implementation using mutex+cond */

int lq_sem_take(lq_sem_t *sem, uint32_t timeout_ms)
{
    if (!sem) {
        return -EINVAL;
    }
    
    int ret = pthread_mutex_lock(&sem->mutex);
    if (ret != 0) {
        return -ret;
    }
    
    if (timeout_ms == 0) {
        /* Non-blocking */
        if (sem->count == 0) {
            pthread_mutex_unlock(&sem->mutex);
            return -EAGAIN;
        }
        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return 0;
    } else if (timeout_ms == UINT32_MAX) {
        /* Blocking forever */
        while (sem->count == 0) {
            ret = pthread_cond_wait(&sem->cond, &sem->mutex);
            if (ret != 0) {
                pthread_mutex_unlock(&sem->mutex);
                return -ret;
            }
        }
        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return 0;
    } else {
        /* Timed wait */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        
        while (sem->count == 0) {
            ret = pthread_cond_timedwait(&sem->cond, &sem->mutex, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&sem->mutex);
                return -EAGAIN;
            }
            if (ret != 0) {
                pthread_mutex_unlock(&sem->mutex);
                return -ret;
            }
        }
        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return 0;
    }
}

#else
/* Other POSIX platforms using standard semaphores */

int lq_sem_take(lq_sem_t *sem, uint32_t timeout_ms)
{
    if (!sem) {
        return -EINVAL;
    }
    
    if (timeout_ms == 0) {
        /* Non-blocking */
        int ret = sem_trywait(&sem->sem);
        if (ret != 0) {
            return errno == EAGAIN ? -EAGAIN : -errno;
        }
        return 0;
    } else if (timeout_ms == UINT32_MAX) {
        /* Blocking forever */
        int ret = sem_wait(&sem->sem);
        return ret == 0 ? 0 : -errno;
    } else {
        /* Timed wait */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        
        int ret = sem_timedwait(&sem->sem, &ts);
        if (ret != 0) {
            return errno == ETIMEDOUT ? -EAGAIN : -errno;
        }
        return 0;
    }
}

#endif

#ifdef __APPLE__
/* macOS implementation using mutex+cond */

int lq_sem_give(lq_sem_t *sem)
{
    if (!sem) {
        return -EINVAL;
    }
    
    int ret = pthread_mutex_lock(&sem->mutex);
    if (ret != 0) {
        return -ret;
    }
    
    /* Check current value against max */
    if (sem->count >= sem->max_count) {
        pthread_mutex_unlock(&sem->mutex);
        return -EINVAL;
    }
    
    sem->count++;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
    
    return 0;
}

#else
/* Other POSIX platforms using standard semaphores */

int lq_sem_give(lq_sem_t *sem)
{
    if (!sem) {
        return -EINVAL;
    }
    
    /* Check current value against max */
    int val;
    sem_getvalue(&sem->sem, &val);
    if (val >= (int)sem->max_count) {
        return -EINVAL;
    }
    
    int ret = sem_post(&sem->sem);
    return ret == 0 ? 0 : -errno;
}

#endif

#ifdef __APPLE__
/* macOS implementation using mutex+cond */

void lq_sem_destroy(lq_sem_t *sem)
{
    if (sem) {
        pthread_cond_destroy(&sem->cond);
        pthread_mutex_destroy(&sem->mutex);
    }
}

#else
/* Other POSIX platforms using standard semaphores */

void lq_sem_destroy(lq_sem_t *sem)
{
    if (sem) {
        sem_destroy(&sem->sem);
    }
}

#endif

/* ============================================================================
 * Atomic implementation
 * ============================================================================ */

void lq_atomic_set(lq_atomic_t *target, int32_t value)
{
    __atomic_store_n(&target->value, value, __ATOMIC_SEQ_CST);
}

int32_t lq_atomic_get(const lq_atomic_t *target)
{
    return __atomic_load_n(&target->value, __ATOMIC_SEQ_CST);
}

int32_t lq_atomic_inc(lq_atomic_t *target)
{
    return __atomic_fetch_add(&target->value, 1, __ATOMIC_SEQ_CST);
}

int32_t lq_atomic_dec(lq_atomic_t *target)
{
    return __atomic_fetch_sub(&target->value, 1, __ATOMIC_SEQ_CST);
}

/* ============================================================================
 * Memory implementation
 * ============================================================================ */

void *lq_malloc(size_t size)
{
    return malloc(size);
}

void lq_free(void *ptr)
{
    free(ptr);
}

/* ============================================================================
 * Logging implementation
 * ============================================================================ */

static const char *log_level_str[] = {
    "ERR",
    "WRN",
    "INF",
    "DBG",
};

void lq_log(lq_log_level_t level, const char *module, const char *fmt, ...)
{
    uint32_t timestamp = lq_platform_uptime_get();
    
    printf("[%08u] <%s> %s: ", timestamp, log_level_str[level], module);
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\n");
}

#endif /* LQ_PLATFORM_NATIVE */
