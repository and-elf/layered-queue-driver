/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hardware input implementation (using lq_queue_core)
 *
 * Queue size can be configured via:
 * 1. LQ_HW_RINGBUFFER_SIZE macro (compile-time)
 * 2. lq_hw_input_init(size) (runtime override, limited by compile-time max)
 *
 * Memory usage: LQ_HW_RINGBUFFER_SIZE * 20 bytes
 */

/* Include generated header if available (contains auto-calculated buffer size) */
#if __has_include("lq_generated.h")
#include "lq_generated.h"
#endif

#include "lq_hw_input.h"
#include "layered_queue_core.h"
#include "lq_platform.h"
#include <string.h>

#ifndef LQ_HW_RINGBUFFER_SIZE
#define LQ_HW_RINGBUFFER_SIZE 128  /* Default: 2.5KB */
#endif

/* Hardware input queue using lq_queue_core */
static lq_queue_t hw_queue;
static struct lq_queue_config hw_config;
static struct lq_queue_data hw_data;
static struct lq_item hw_items[LQ_HW_RINGBUFFER_SIZE];

int lq_hw_input_init(size_t size)
{
    /* Use provided size, but cap to compile-time maximum */
    if (size == 0 || size > LQ_HW_RINGBUFFER_SIZE) {
        size = LQ_HW_RINGBUFFER_SIZE;
    }

    /* Configure queue */
    hw_config.capacity = (uint32_t)size;
    hw_config.drop_policy = LQ_DROP_OLDEST;  /* ISR context: drop old data if full */
    hw_config.priority = 0;

    return lq_queue_init(&hw_queue, &hw_config, &hw_data, hw_items);
}

void lq_hw_push(enum lq_hw_source src, uint32_t value)
{
    struct lq_item item = {
        .timestamp = (uint32_t)(lq_platform_uptime_get()), /* Already in ms */
        .value = (int32_t)value,
        .status = LQ_OK,
        .source_id = (uint8_t)src,
        .sequence = 0, /* Will be set by queue */
    };

    /* ISR-safe: use 0 timeout (no blocking) */
    lq_queue_push(&hw_queue, &item, 0);
}

int lq_hw_pop(struct lq_hw_sample *sample)
{
    struct lq_item item;

    /* Non-blocking pop */
    int ret = lq_queue_pop(&hw_queue, &item, 0);
    if (ret == 0) {
        sample->src = (enum lq_hw_source)item.source_id;
        sample->value = (uint32_t)item.value;
        sample->timestamp = (uint64_t)item.timestamp * 1000; /* ms to us */
    }

    return ret;
}

size_t lq_hw_pending(void)
{
    return (size_t)lq_queue_count(&hw_queue);
}
