/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hardware input implementation
 */

#include "lq_hw_input.h"
#include "lq_platform.h"

#ifdef LQ_PLATFORM_NATIVE

#include <pthread.h>

#ifndef LQ_HW_RINGBUFFER_SIZE
#define LQ_HW_RINGBUFFER_SIZE 128
#endif

/* Simple ringbuffer */
static struct {
    struct lq_hw_sample buffer[LQ_HW_RINGBUFFER_SIZE];
    size_t write_idx;
    size_t read_idx;
    size_t count;
    pthread_mutex_t mutex;
} hw_ringbuf;

int lq_hw_input_init(size_t size)
{
    (void)size; /* Use compile-time size */
    
    hw_ringbuf.write_idx = 0;
    hw_ringbuf.read_idx = 0;
    hw_ringbuf.count = 0;
    
    return pthread_mutex_init(&hw_ringbuf.mutex, NULL);
}

void lq_hw_push(enum lq_hw_source src, uint32_t value)
{
    pthread_mutex_lock(&hw_ringbuf.mutex);
    
    if (hw_ringbuf.count < LQ_HW_RINGBUFFER_SIZE) {
        struct lq_hw_sample *sample = &hw_ringbuf.buffer[hw_ringbuf.write_idx];
        sample->src = src;
        sample->value = value;
        sample->timestamp = lq_platform_uptime_get() * 1000; /* ms to us */
        
        hw_ringbuf.write_idx = (hw_ringbuf.write_idx + 1) % LQ_HW_RINGBUFFER_SIZE;
        hw_ringbuf.count++;
    }
    
    pthread_mutex_unlock(&hw_ringbuf.mutex);
}

int lq_hw_pop(struct lq_hw_sample *sample)
{
    int ret = -1; /* -EAGAIN */
    
    pthread_mutex_lock(&hw_ringbuf.mutex);
    
    if (hw_ringbuf.count > 0) {
        *sample = hw_ringbuf.buffer[hw_ringbuf.read_idx];
        hw_ringbuf.read_idx = (hw_ringbuf.read_idx + 1) % LQ_HW_RINGBUFFER_SIZE;
        hw_ringbuf.count--;
        ret = 0;
    }
    
    pthread_mutex_unlock(&hw_ringbuf.mutex);
    
    return ret;
}

size_t lq_hw_pending(void)
{
    size_t count;
    
    pthread_mutex_lock(&hw_ringbuf.mutex);
    count = hw_ringbuf.count;
    pthread_mutex_unlock(&hw_ringbuf.mutex);
    
    return count;
}

#endif /* LQ_PLATFORM_NATIVE */

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>

#ifndef CONFIG_LQ_HW_RINGBUFFER_SIZE
#define CONFIG_LQ_HW_RINGBUFFER_SIZE 128
#endif

/* Zephyr ringbuffer */
static struct {
    struct lq_hw_sample buffer[CONFIG_LQ_HW_RINGBUFFER_SIZE];
    size_t write_idx;
    size_t read_idx;
    size_t count;
    struct k_mutex mutex;
} hw_ringbuf;

int lq_hw_input_init(size_t size)
{
    (void)size;
    
    hw_ringbuf.write_idx = 0;
    hw_ringbuf.read_idx = 0;
    hw_ringbuf.count = 0;
    
    return k_mutex_init(&hw_ringbuf.mutex);
}

void lq_hw_push(enum lq_hw_source src, uint32_t value)
{
    k_mutex_lock(&hw_ringbuf.mutex, K_FOREVER);
    
    if (hw_ringbuf.count < CONFIG_LQ_HW_RINGBUFFER_SIZE) {
        struct lq_hw_sample *sample = &hw_ringbuf.buffer[hw_ringbuf.write_idx];
        sample->src = src;
        sample->value = value;
        sample->timestamp = k_uptime_get() * 1000; /* ms to us */
        
        hw_ringbuf.write_idx = (hw_ringbuf.write_idx + 1) % CONFIG_LQ_HW_RINGBUFFER_SIZE;
        hw_ringbuf.count++;
    }
    
    k_mutex_unlock(&hw_ringbuf.mutex);
}

int lq_hw_pop(struct lq_hw_sample *sample)
{
    int ret = -EAGAIN;
    
    k_mutex_lock(&hw_ringbuf.mutex, K_FOREVER);
    
    if (hw_ringbuf.count > 0) {
        *sample = hw_ringbuf.buffer[hw_ringbuf.read_idx];
        hw_ringbuf.read_idx = (hw_ringbuf.read_idx + 1) % CONFIG_LQ_HW_RINGBUFFER_SIZE;
        hw_ringbuf.count--;
        ret = 0;
    }
    
    k_mutex_unlock(&hw_ringbuf.mutex);
    
    return ret;
}

size_t lq_hw_pending(void)
{
    size_t count;
    
    k_mutex_lock(&hw_ringbuf.mutex, K_FOREVER);
    count = hw_ringbuf.count;
    k_mutex_unlock(&hw_ringbuf.mutex);
    
    return count;
}

#endif /* __ZEPHYR__ */
