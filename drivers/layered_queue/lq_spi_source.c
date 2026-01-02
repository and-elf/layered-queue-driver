/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_lq_spi_source

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include "layered_queue_core.h"
#include "lq_spi_source.h"
#include <zephyr/drivers/layered_queue.h>
#include <zephyr/drivers/layered_queue_internal.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lq_spi_source, CONFIG_LQ_LOG_LEVEL);

struct lq_spi_source_zephyr_config {
    struct spi_dt_spec spi;
    const struct device *output_queue;
    struct lq_spi_source_config core_config;
    const struct lq_expected_value *expected;
    uint8_t num_expected;
};

struct lq_spi_source_zephyr_data {
    struct k_work_delayable poll_work;
    uint32_t sequence;
    uint8_t rx_buffer[4];  /* Max 4 bytes for int32 */
};

static void lq_spi_source_poll_work(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct lq_spi_source_zephyr_data *data =
        CONTAINER_OF(dwork, struct lq_spi_source_zephyr_data, poll_work);
    const struct device *dev = CONTAINER_OF(data, const struct device, data);
    const struct lq_spi_source_zephyr_config *config = dev->config;

    /* Read from SPI */
    const struct spi_buf rx_buf = {
        .buf = data->rx_buffer,
        .len = config->core_config.read_length,
    };
    const struct spi_buf_set rx_bufs = {
        .buffers = &rx_buf,
        .count = 1,
    };

    int ret = spi_read_dt(&config->spi, &rx_bufs);
    if (ret != 0) {
        LOG_ERR("SPI read failed: %d", ret);
        goto reschedule;
    }

    /* Process data using platform-independent logic */
    int32_t value;
    enum lq_status status;
    ret = lq_spi_source_process(data->rx_buffer,
                                config->core_config.read_length,
                                &config->core_config,
                                &value,
                                &status);
    if (ret != 0) {
        LOG_ERR("SPI process failed: %d", ret);
        goto reschedule;
    }

    /* Create queue item */
    struct lq_item item = {
        .timestamp = k_uptime_get_32(),
        .value = value,
        .status = status,
        .source_id = 0,  /* TODO: Get from DT */
        .sequence = data->sequence++,
    };

    /* Push to output queue */
    ret = lq_push(config->output_queue, &item, K_NO_WAIT);
    if (ret != 0) {
        LOG_WRN("Failed to push SPI value: %d", ret);
    }

reschedule:
    /* Reschedule */
    k_work_schedule(dwork, K_MSEC(config->core_config.poll_interval_ms));
}

static int lq_spi_source_init(const struct device *dev)
{
    struct lq_spi_source_zephyr_data *data = dev->data;
    const struct lq_spi_source_zephyr_config *config = dev->config;

    if (!spi_is_ready_dt(&config->spi)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }

    if (!device_is_ready(config->output_queue)) {
        LOG_ERR("Output queue not ready");
        return -ENODEV;
    }

    /* Initialize work */
    k_work_init_delayable(&data->poll_work, lq_spi_source_poll_work);
    k_work_schedule(&data->poll_work, K_MSEC(config->core_config.poll_interval_ms));

    LOG_INF("SPI source %s initialized (interval=%ums, len=%u)",
            dev->name,
            config->core_config.poll_interval_ms,
            config->core_config.read_length);

    return 0;
}

/* TODO: Add device tree macros to extract expected values */

#define LQ_SPI_SOURCE_DEFINE(inst) \
    static struct lq_spi_source_zephyr_data lq_spi_source_data_##inst; \
    \
    static const struct lq_spi_source_zephyr_config lq_spi_source_config_##inst = { \
        .spi = SPI_DT_SPEC_INST_GET(inst, SPI_WORD_SET(8), 0), \
        .output_queue = DEVICE_DT_GET(DT_INST_PHANDLE(inst, output_queue)), \
        .core_config = { \
            .poll_interval_ms = DT_INST_PROP_OR(inst, poll_interval_ms, 100), \
            .read_length = DT_INST_PROP_OR(inst, read_length, 2), \
            .expected = NULL, /* TODO: Parse from DT */ \
            .num_expected = 0, \
            .default_status = LQ_ERROR, \
        }, \
    }; \
    \
    DEVICE_DT_INST_DEFINE(inst, \
                          lq_spi_source_init, \
                          NULL, \
                          &lq_spi_source_data_##inst, \
                          &lq_spi_source_config_##inst, \
                          POST_KERNEL, \
                          CONFIG_APPLICATION_INIT_PRIORITY, \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_SPI_SOURCE_DEFINE)
