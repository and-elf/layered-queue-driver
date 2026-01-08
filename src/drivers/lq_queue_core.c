/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Core queue implementation (platform-independent)
 */

#include "layered_queue_core.h"

#define MODULE_NAME "lq_queue"

int lq_queue_init(lq_queue_t *queue,
                  const struct lq_queue_config *config,
                  struct lq_queue_data *data,
                  struct lq_item *items_buffer)
{
    if (!queue || !config || !data || !items_buffer) {
        return -EINVAL;
    }

    if (config->capacity == 0) {
        return -EINVAL;
    }

    /* Initialize queue structure */
    queue->config = config;
    queue->data = data;

    /* Initialize runtime data */
    data->items = items_buffer;
    data->read_idx = 0;
    data->write_idx = 0;
    lq_atomic_set(&data->count, 0);
    data->sequence = 0;
    memset(&data->stats, 0, sizeof(data->stats));

    /* Initialize synchronization primitives */
    int ret = lq_sem_init(&data->sem_read, 0, config->capacity);
    if (ret != 0) {
        return ret;
    }

    ret = lq_sem_init(&data->sem_write, config->capacity, config->capacity);
    if (ret != 0) {
        lq_sem_destroy(&data->sem_read);
        return ret;
    }

    ret = lq_mutex_init(&data->mutex);
    if (ret != 0) {
        lq_sem_destroy(&data->sem_read);
        lq_sem_destroy(&data->sem_write);
        return ret;
    }

    LQ_LOG_INF(MODULE_NAME, "Queue initialized (capacity=%u, policy=%d)",
               config->capacity, config->drop_policy);

    return 0;
}

void lq_queue_destroy(lq_queue_t *queue)
{
    if (!queue || !queue->data) {
        return;
    }

    lq_mutex_destroy(&queue->data->mutex);
    lq_sem_destroy(&queue->data->sem_read);
    lq_sem_destroy(&queue->data->sem_write);
}

int lq_queue_push(lq_queue_t *queue,
                  const struct lq_item *item,
                  uint32_t timeout_ms)
{
    if (!queue || !queue->config || !queue->data || !item) {
        return -EINVAL;
    }

    const struct lq_queue_config *config = queue->config;
    struct lq_queue_data *data = queue->data;

    int ret = lq_mutex_lock(&data->mutex);
    if (ret != 0) {
        return ret;
    }

    /* Check if queue is full */
    if (lq_atomic_get(&data->count) >= (int32_t)config->capacity) {
        if (config->drop_policy == LQ_DROP_NEWEST) {
            lq_mutex_unlock(&data->mutex);
            data->stats.items_dropped++;
            LQ_LOG_WRN(MODULE_NAME, "Queue full, dropping newest");
            return -ENOMEM;
        } else if (config->drop_policy == LQ_DROP_OLDEST) {
            /* Remove oldest item */
            data->read_idx = (data->read_idx + 1) % config->capacity;
            lq_atomic_dec(&data->count);
            data->stats.items_dropped++;
            LQ_LOG_DBG(MODULE_NAME, "Queue full, dropped oldest");
        } else {
            /* Block until space available */
            lq_mutex_unlock(&data->mutex);
            ret = lq_sem_take(&data->sem_write, timeout_ms);
            if (ret != 0) {
                return ret;
            }
            ret = lq_mutex_lock(&data->mutex);
            if (ret != 0) {
                return ret;
            }
        }
    }

    /* Insert item */
    memcpy(&data->items[data->write_idx], item, sizeof(struct lq_item));
    data->items[data->write_idx].timestamp = lq_platform_uptime_get();
    data->items[data->write_idx].sequence = data->sequence++;
    data->write_idx = (data->write_idx + 1) % config->capacity;
    lq_atomic_inc(&data->count);
    data->stats.items_written++;

    /* Update peak usage */
    uint32_t current = (uint32_t)lq_atomic_get(&data->count);
    if (current > data->stats.peak_usage) {
        data->stats.peak_usage = current;
    }

    /* Signal readers */
    lq_sem_give(&data->sem_read);

    lq_mutex_unlock(&data->mutex);
    return 0;
}

int lq_queue_pop(lq_queue_t *queue,
                 struct lq_item *item,
                 uint32_t timeout_ms)
{
    if (!queue || !queue->config || !queue->data || !item) {
        return -EINVAL;
    }

    const struct lq_queue_config *config = queue->config;
    struct lq_queue_data *data = queue->data;

    /* Wait for data */
    int ret = lq_sem_take(&data->sem_read, timeout_ms);
    if (ret != 0) {
        return ret;
    }

    ret = lq_mutex_lock(&data->mutex);
    if (ret != 0) {
        return ret;
    }

    /* Read item */
    memcpy(item, &data->items[data->read_idx], sizeof(struct lq_item));
    data->read_idx = (data->read_idx + 1) % config->capacity;
    lq_atomic_dec(&data->count);
    data->stats.items_read++;

    /* Signal writers if blocking */
    if (config->drop_policy == LQ_BLOCK) {
        lq_sem_give(&data->sem_write);
    }

    lq_mutex_unlock(&data->mutex);
    return 0;
}

int lq_queue_peek(lq_queue_t *queue,
                  struct lq_item *item)
{
    if (!queue || !queue->config || !queue->data || !item) {
        return -EINVAL;
    }

    const struct lq_queue_config *config = queue->config;
    struct lq_queue_data *data = queue->data;

    int ret = lq_mutex_lock(&data->mutex);
    if (ret != 0) {
        return ret;
    }

    if (lq_atomic_get(&data->count) == 0) {
        lq_mutex_unlock(&data->mutex);
        return -ENODATA;
    }

    /* Get most recent item (one before write index) */
    uint32_t peek_idx = (data->write_idx == 0) ?
                        config->capacity - 1 : data->write_idx - 1;
    memcpy(item, &data->items[peek_idx], sizeof(struct lq_item));

    lq_mutex_unlock(&data->mutex);
    return 0;
}

int lq_queue_get_stats(lq_queue_t *queue,
                       struct lq_stats *stats)
{
    if (!queue || !queue->data || !stats) {
        return -EINVAL;
    }

    struct lq_queue_data *data = queue->data;

    int ret = lq_mutex_lock(&data->mutex);
    if (ret != 0) {
        return ret;
    }

    memcpy(stats, &data->stats, sizeof(struct lq_stats));
    stats->items_current = (uint32_t)lq_atomic_get(&data->count);

    lq_mutex_unlock(&data->mutex);
    return 0;
}

uint32_t lq_queue_count(lq_queue_t *queue)
{
    if (!queue || !queue->data) {
        return 0;
    }

    return (uint32_t)lq_atomic_get(&queue->data->count);
}
