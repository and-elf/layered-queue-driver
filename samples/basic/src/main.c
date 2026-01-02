/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example application demonstrating layered queue usage
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/layered_queue.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lq_example, LOG_LEVEL_INF);

/* Get queue devices from device tree */
#define PRESSURE_QUEUE DT_NODELABEL(q_pressure)
#define STATE_QUEUE DT_NODELABEL(q_state)
#define MERGED_QUEUE DT_NODELABEL(q_merged)

static const struct device *pressure_queue = DEVICE_DT_GET(PRESSURE_QUEUE);
static const struct device *state_queue = DEVICE_DT_GET(STATE_QUEUE);
static const struct device *merged_queue = DEVICE_DT_GET(MERGED_QUEUE);

/* Callback for pressure updates */
static void pressure_callback(const struct device *dev,
                              const struct lq_item *item,
                              void *user_data)
{
    LOG_INF("Pressure: %d (status=%d)", item->value, item->status);

    if (item->status == LQ_DEGRADED) {
        LOG_WRN("Pressure in degraded range!");
    } else if (item->status != LQ_OK) {
        LOG_ERR("Pressure sensor fault!");
    }
}

/* Consumer thread for merged data */
static void merged_consumer_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Merged consumer started");

    while (1) {
        struct lq_item item;
        int ret = lq_pop(merged_queue, &item, K_FOREVER);

        if (ret == 0) {
            const char *status_str;
            switch (item.status) {
            case LQ_OK:
                status_str = "OK";
                break;
            case LQ_DEGRADED:
                status_str = "DEGRADED";
                break;
            case LQ_OUT_OF_RANGE:
                status_str = "OUT_OF_RANGE";
                break;
            case LQ_INCONSISTENT:
                status_str = "INCONSISTENT";
                break;
            default:
                status_str = "UNKNOWN";
                break;
            }

            LOG_INF("Merged value: %d [%s] @ %u ms",
                    item.value, status_str, item.timestamp);
        }
    }
}

K_THREAD_DEFINE(merged_consumer, 2048,
                merged_consumer_thread, NULL, NULL, NULL,
                5, 0, 0);

int main(void)
{
    LOG_INF("Layered Queue Example Application");

    /* Verify devices are ready */
    if (!device_is_ready(pressure_queue)) {
        LOG_ERR("Pressure queue not ready");
        return -1;
    }

    if (!device_is_ready(state_queue)) {
        LOG_ERR("State queue not ready");
        return -1;
    }

    if (!device_is_ready(merged_queue)) {
        LOG_ERR("Merged queue not ready");
        return -1;
    }

    LOG_INF("All queues ready");

    /* Register callback for pressure updates */
    lq_register_callback(pressure_queue, pressure_callback, NULL);

    /* Monitor queue statistics */
    while (1) {
        k_sleep(K_SECONDS(5));

        struct lq_stats stats;

        lq_get_stats(pressure_queue, &stats);
        LOG_INF("Pressure queue: %u/%u items, %u dropped, peak %u",
                stats.items_current,
                16, /* capacity from DT */
                stats.items_dropped,
                stats.peak_usage);

        lq_get_stats(merged_queue, &stats);
        LOG_INF("Merged queue: %u written, %u read",
                stats.items_written,
                stats.items_read);
    }

    return 0;
}
