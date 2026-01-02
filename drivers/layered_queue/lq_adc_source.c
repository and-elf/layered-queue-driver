/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_lq_adc_source

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/layered_queue.h>
#include <zephyr/drivers/layered_queue_internal.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lq_adc_source, CONFIG_LQ_LOG_LEVEL);

static void lq_adc_source_poll_work(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct lq_adc_source_data *data =
        CONTAINER_OF(dwork, struct lq_adc_source_data, poll_work);
    const struct device *dev = CONTAINER_OF(data, struct device, data);
    const struct lq_adc_source_config *config = dev->config;

    /* TODO: Read ADC channel */
    int32_t adc_value = 0; /* Placeholder */

    /* Add to averaging buffer */
    data->samples[data->sample_idx++] = adc_value;

    /* Compute average when buffer is full */
    if (data->sample_idx >= config->averaging) {
        int64_t sum = 0;
        for (uint8_t i = 0; i < config->averaging; i++) {
            sum += data->samples[i];
        }
        int32_t avg_value = (int32_t)(sum / config->averaging);

        /* Validate against ranges */
        enum lq_status status = lq_validate_range(avg_value,
                                                    config->ranges,
                                                    config->num_ranges,
                                                    LQ_ERROR);

        /* Create queue item */
        struct lq_item item = {
            .timestamp = k_uptime_get_32(),
            .value = avg_value,
            .status = status,
            .source_id = 0, /* TODO: Get from device tree */
            .sequence = data->sequence++,
        };

        /* Push to output queue */
        int ret = lq_push(config->output_queue, &item, K_NO_WAIT);
        if (ret != 0) {
            LOG_WRN("Failed to push ADC value: %d", ret);
        }

        /* Reset averaging buffer */
        data->sample_idx = 0;
    }

    /* Reschedule */
    k_work_schedule(dwork, K_MSEC(config->poll_interval_ms));
}

static int lq_adc_source_init(const struct device *dev)
{
    struct lq_adc_source_data *data = dev->data;
    const struct lq_adc_source_config *config = dev->config;

    if (!device_is_ready(config->adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    if (!device_is_ready(config->output_queue)) {
        LOG_ERR("Output queue not ready");
        return -ENODEV;
    }

    /* TODO: Configure ADC channel */

    /* Initialize work */
    k_work_init_delayable(&data->poll_work, lq_adc_source_poll_work);
    k_work_schedule(&data->poll_work, K_MSEC(config->poll_interval_ms));

    LOG_INF("ADC source %s initialized (ch=%u, interval=%ums)",
            dev->name, config->channel, config->poll_interval_ms);

    return 0;
}

/* TODO: Add device tree macros to extract ranges and instantiate drivers */

#define LQ_ADC_SOURCE_DEFINE(inst) \
    /* Placeholder - needs DT range parsing */ \
    static int32_t lq_adc_samples_##inst[DT_INST_PROP_OR(inst, averaging, 1)]; \
    \
    static struct lq_adc_source_data lq_adc_source_data_##inst = { \
        .samples = lq_adc_samples_##inst, \
    }; \
    \
    static const struct lq_adc_source_config lq_adc_source_config_##inst = { \
        .adc_dev = DEVICE_DT_GET(DT_INST_PHANDLE(inst, adc)), \
        .channel = DT_INST_PROP(inst, channel), \
        .output_queue = DEVICE_DT_GET(DT_INST_PHANDLE(inst, output_queue)), \
        .poll_interval_ms = DT_INST_PROP_OR(inst, poll_interval_ms, 100), \
        .averaging = DT_INST_PROP_OR(inst, averaging, 1), \
        /* TODO: Parse ranges from child nodes */ \
        .ranges = NULL, \
        .num_ranges = 0, \
    }; \
    \
    DEVICE_DT_INST_DEFINE(inst, \
                          lq_adc_source_init, \
                          NULL, \
                          &lq_adc_source_data_##inst, \
                          &lq_adc_source_config_##inst, \
                          POST_KERNEL, \
                          CONFIG_APPLICATION_INIT_PRIORITY, \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_ADC_SOURCE_DEFINE)
