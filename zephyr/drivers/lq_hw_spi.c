/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,hw-spi-input
 */

#define DT_DRV_COMPAT lq_hw_spi_input

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

#include "lq_hw_input.h"
#include "lq_system_health.h"

LOG_MODULE_REGISTER(lq_hw_spi, CONFIG_LQ_LOG_LEVEL);

struct lq_hw_spi_data {
	const struct device *spi_dev;
	struct spi_config spi_cfg;
	enum lq_hw_source hw_source;
	struct k_work sample_work;
	struct k_timer sample_timer;
	uint8_t rx_buffer[4];
	int health_index;  /* Health registry index */
};

struct lq_hw_spi_config {
	const struct device *spi_dev;
	uint16_t slave;
	uint32_t frequency;
	uint32_t stale_us;
};

static void lq_hw_spi_sample_work_handler(struct k_work *work)
{
	struct lq_hw_spi_data *data = CONTAINER_OF(work, struct lq_hw_spi_data, sample_work);
	
	const struct spi_buf tx_buf = {
		.buf = NULL,
		.len = 0,
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1,
	};
	
	struct spi_buf rx_buf = {
		.buf = data->rx_buffer,
		.len = sizeof(data->rx_buffer),
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1,
	};
	
	int ret = spi_transceive(data->spi_dev, &data->spi_cfg, &tx, &rx);
	if (ret == 0) {
		/* Combine bytes into 32-bit value (big-endian) */
		uint32_t value = ((uint32_t)data->rx_buffer[0] << 24) |
		                 ((uint32_t)data->rx_buffer[1] << 16) |
		                 ((uint32_t)data->rx_buffer[2] << 8) |
		                 ((uint32_t)data->rx_buffer[3]);
		lq_hw_push(data->hw_source, value);
		
		/* Update health status - device operational */
		lq_health_update(data->health_index, LQ_HEALTH_OK, 0);
	} else {
		LOG_ERR("SPI read failed: %d - DEGRADED", ret);
		
		/* Report runtime failure to health monitor */
		lq_health_update(data->health_index, LQ_HEALTH_DEGRADED, ret);
	}
}

static void lq_hw_spi_timer_handler(struct k_timer *timer)
{
	struct lq_hw_spi_data *data = CONTAINER_OF(timer, struct lq_hw_spi_data, sample_timer);
	k_work_submit(&data->sample_work);
}

static int lq_hw_spi_init(const struct device *dev)
{
	struct lq_hw_spi_data *data = dev->data;
	const struct lq_hw_spi_config *config = dev->config;
	
	/* Check if SPI bus is ready */
	if (!device_is_ready(config->spi_dev)) {
		LOG_ERR("SPI device not ready - CRITICAL FAILURE");
		
		/* Register failure with health monitor - SAFETY CRITICAL */
		data->health_index = lq_health_register(
			dev->name,
			LQ_HEALTH_INIT_FAILED,
			-ENODEV
		);
		
		return -ENODEV;  /* System will know via health monitor */
	}
	
	data->spi_dev = config->spi_dev;
	data->spi_cfg.frequency = config->frequency;
	data->spi_cfg.operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8);
	data->spi_cfg.slave = config->slave;
	
	data->hw_source = LQ_HW_SPI0 + DT_INST_NODE_IDX(dev);
	
	/* Register healthy status with monitoring system */
	data->health_index = lq_health_register(
		dev->name,
		LQ_HEALTH_OK,
		0
	);
	
	k_work_init(&data->sample_work, lq_hw_spi_sample_work_handler);
	k_timer_init(&data->sample_timer, lq_hw_spi_timer_handler, NULL);
	k_timer_start(&data->sample_timer, K_MSEC(10), K_MSEC(10));
	
	LOG_INF("Initialized SPI input -> hw_source %d", data->hw_source);
	
	return 0;
}

#define LQ_HW_SPI_DEFINE(inst)						\
	static struct lq_hw_spi_data lq_hw_spi_data_##inst;		\
									\
	static const struct lq_hw_spi_config lq_hw_spi_config_##inst = { \
		.spi_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),		\
		.slave = DT_INST_REG_ADDR(inst),			\
		.frequency = DT_INST_PROP_OR(inst, spi_max_frequency, 1000000), \
		.stale_us = DT_INST_PROP_OR(inst, stale_us, 100000),	\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_hw_spi_init,				\
			      NULL,					\
			      &lq_hw_spi_data_##inst,			\
			      &lq_hw_spi_config_##inst,			\
			      POST_KERNEL,				\
			      CONFIG_LQ_HW_SPI_INIT_PRIORITY,		\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_HW_SPI_DEFINE)
