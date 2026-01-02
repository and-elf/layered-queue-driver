/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_lq_merge_voter

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/layered_queue.h>
#include <zephyr/drivers/layered_queue_internal.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lq_merge_voter, CONFIG_LQ_LOG_LEVEL);

static void lq_merge_work(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct lq_merge_data *data =
        CONTAINER_OF(dwork, struct lq_merge_data, merge_work);
    const struct device *dev = CONTAINER_OF(data, struct device, data);
    const struct lq_merge_config *config = dev->config;

    uint32_t current_time = k_uptime_get_32();
    int32_t values[config->num_inputs];
    uint8_t valid_inputs = 0;

    /* Collect latest values from all input queues */
    for (uint8_t i = 0; i < config->num_inputs; i++) {
        struct lq_item item;
        int ret = lq_pop(config->input_queues[i], &item, K_NO_WAIT);

        if (ret == 0) {
            values[valid_inputs++] = item.value;
            data->last_values[i] = item.value;
            data->last_timestamps[i] = item.timestamp;
        } else {
            /* Use stale value if within timeout */
            uint32_t age = current_time - data->last_timestamps[i];
            if (age < config->timeout_ms) {
                values[valid_inputs++] = data->last_values[i];
            }
        }
    }

    /* Only proceed if we have at least one input */
    if (valid_inputs > 0) {
        int32_t result;
        enum lq_status status;

        /* Perform voting */
        int ret = lq_vote(values, valid_inputs,
                         config->voting_method,
                         config->tolerance,
                         &result, &status);

        if (ret != 0) {
            LOG_ERR("Voting failed: %d", ret);
            k_work_schedule(dwork, K_MSEC(10));
            return;
        }

        /* Apply violation status if configured */
        if (status == LQ_INCONSISTENT) {
            status = config->status_if_violation;
        }

        /* Validate against expected range if configured */
        if (config->expected_range) {
            if (result < config->expected_range->min ||
                result > config->expected_range->max) {
                status = config->expected_range->status;
            }
        }

        /* Check for timeout (not all inputs fresh) */
        if (valid_inputs < config->num_inputs) {
            if (status == LQ_OK) {
                status = LQ_TIMEOUT;
            }
        }

        /* Create output item */
        struct lq_item item = {
            .timestamp = current_time,
            .value = result,
            .status = status,
            .source_id = 0xFF, /* Merged */
            .sequence = data->sequence++,
        };

        /* Push to output queue */
        ret = lq_push(config->output_queue, &item, K_NO_WAIT);
        if (ret != 0) {
            LOG_WRN("Failed to push merged value: %d", ret);
        }
    }

    /* Reschedule - use shorter interval than inputs */
    k_work_schedule(dwork, K_MSEC(10));
}

static int lq_merge_voter_init(const struct device *dev)
{
    struct lq_merge_data *data = dev->data;
    const struct lq_merge_config *config = dev->config;

    /* Verify all input queues are ready */
    for (uint8_t i = 0; i < config->num_inputs; i++) {
        if (!device_is_ready(config->input_queues[i])) {
            LOG_ERR("Input queue %u not ready", i);
            return -ENODEV;
        }
    }

    if (!device_is_ready(config->output_queue)) {
        LOG_ERR("Output queue not ready");
        return -ENODEV;
    }

    /* Initialize timestamps to current time */
    uint32_t now = k_uptime_get_32();
    for (uint8_t i = 0; i < config->num_inputs; i++) {
        data->last_timestamps[i] = now;
        data->last_values[i] = 0;
    }

    /* Start merge work */
    k_work_init_delayable(&data->merge_work, lq_merge_work);
    k_work_schedule(&data->merge_work, K_MSEC(50));

    LOG_INF("Merge voter %s initialized (%u inputs, method=%d)",
            dev->name, config->num_inputs, config->voting_method);

    return 0;
}

/* TODO: Add device tree macros to instantiate merge drivers */

#define LQ_MERGE_VOTER_DEFINE(inst) \
    /* Placeholder - needs DT input queue and range parsing */ \
    static int32_t lq_merge_values_##inst[2]; /* TODO: Get from DT */ \
    static uint32_t lq_merge_timestamps_##inst[2]; \
    \
    static struct lq_merge_data lq_merge_data_##inst = { \
        .last_values = lq_merge_values_##inst, \
        .last_timestamps = lq_merge_timestamps_##inst, \
    }; \
    \
    static const struct lq_merge_config lq_merge_config_##inst = { \
        /* TODO: Parse from DT */ \
        .input_queues = NULL, \
        .num_inputs = 0, \
        .output_queue = DEVICE_DT_GET(DT_INST_PHANDLE(inst, output_queue)), \
        .voting_method = LQ_VOTE_MEDIAN, \
        .tolerance = DT_INST_PROP_OR(inst, tolerance, 0), \
        .status_if_violation = DT_INST_PROP_OR(inst, status_if_violation, LQ_OUT_OF_RANGE), \
        .timeout_ms = DT_INST_PROP_OR(inst, timeout_ms, 1000), \
        .expected_range = NULL, \
    }; \
    \
    DEVICE_DT_INST_DEFINE(inst, \
                          lq_merge_voter_init, \
                          NULL, \
                          &lq_merge_data_##inst, \
                          &lq_merge_config_##inst, \
                          POST_KERNEL, \
                          CONFIG_APPLICATION_INIT_PRIORITY, \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_MERGE_VOTER_DEFINE)
