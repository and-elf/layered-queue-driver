/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_lq_queue

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "layered_queue_core.h"
#include <zephyr/drivers/layered_queue.h>
#include <zephyr/drivers/layered_queue_internal.h>

/* Queue device driver wrapper for Zephyr device tree */

struct lq_queue_dt_config {
    struct lq_queue_config core_config;
};

struct lq_queue_dt_data {
    lq_queue_t queue;
    struct lq_queue_data core_data;
    struct lq_item *items;
};

static int lq_queue_dt_push(const struct device *dev,
                            const struct lq_item *item,
                            k_timeout_t timeout)
{
    struct lq_queue_dt_data *data = dev->data;
    uint32_t timeout_ms = k_timeout_to_ms_ceil(&timeout);
    return lq_queue_push(&data->queue, item, timeout_ms);
}

static int lq_queue_dt_pop(const struct device *dev,
                           struct lq_item *item,
                           k_timeout_t timeout)
{
    struct lq_queue_dt_data *data = dev->data;
    uint32_t timeout_ms = k_timeout_to_ms_ceil(&timeout);
    return lq_queue_pop(&data->queue, item, timeout_ms);
}

static int lq_queue_dt_peek(const struct device *dev,
                            struct lq_item *item)
{
    struct lq_queue_dt_data *data = dev->data;
    return lq_queue_peek(&data->queue, item);
}

static int lq_queue_dt_get_stats(const struct device *dev,
                                 struct lq_stats *stats)
{
    struct lq_queue_dt_data *data = dev->data;
    return lq_queue_get_stats(&data->queue, stats);
}

static int lq_queue_dt_register_callback(const struct device *dev,
                                         lq_callback_t callback,
                                         void *user_data)
{
    /* TODO: Implement callback support */
    return -ENOTSUP;
}

static const struct lq_driver_api lq_queue_dt_api = {
    .push = lq_queue_dt_push,
    .pop = lq_queue_dt_pop,
    .peek = lq_queue_dt_peek,
    .get_stats = lq_queue_dt_get_stats,
    .register_callback = lq_queue_dt_register_callback,
};

static int lq_queue_dt_init(const struct device *dev)
{
    struct lq_queue_dt_data *data = dev->data;
    const struct lq_queue_dt_config *config = dev->config;

    return lq_queue_init(&data->queue, &config->core_config,
                        &data->core_data, data->items);
}

#define LQ_QUEUE_DROP_POLICY(node_id) \
    COND_CODE_1(DT_NODE_HAS_PROP(node_id, drop_policy), \
        (DT_STRING_TOKEN(node_id, drop_policy) == drop_oldest ? LQ_DROP_OLDEST : \
         DT_STRING_TOKEN(node_id, drop_policy) == drop_newest ? LQ_DROP_NEWEST : \
         LQ_BLOCK), \
        (LQ_DROP_OLDEST))

#define LQ_QUEUE_DEFINE(inst) \
    static struct lq_item lq_queue_items_##inst[DT_INST_PROP(inst, capacity)]; \
    \
    static struct lq_queue_dt_data lq_queue_data_##inst = { \
        .items = lq_queue_items_##inst, \
    }; \
    \
    static const struct lq_queue_dt_config lq_queue_config_##inst = { \
        .core_config = { \
            .capacity = DT_INST_PROP(inst, capacity), \
            .drop_policy = LQ_QUEUE_DROP_POLICY(DT_DRV_INST(inst)), \
            .priority = DT_INST_PROP_OR(inst, priority, 0), \
        }, \
    }; \
    \
    DEVICE_DT_INST_DEFINE(inst, \
                          lq_queue_dt_init, \
                          NULL, \
                          &lq_queue_data_##inst, \
                          &lq_queue_config_##inst, \
                          POST_KERNEL, \
                          CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
                          &lq_queue_dt_api);

DT_INST_FOREACH_STATUS_OKAY(LQ_QUEUE_DEFINE)
