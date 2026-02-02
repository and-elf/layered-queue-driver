/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,hw-sensor-input
 */

#define DT_DRV_COMPAT lq_hw_sensor_input

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "lq_hw_input.h"

LOG_MODULE_REGISTER(lq_hw_sensor, CONFIG_LQ_LOG_LEVEL);

struct lq_hw_sensor_data {
	const struct device *sensor_dev;
	enum sensor_channel channel;
	enum lq_hw_source hw_source;
	struct k_work sample_work;
	struct k_timer sample_timer;
	int32_t scale_factor;
};

struct lq_hw_sensor_config {
	const struct device *sensor_dev;
	enum sensor_channel channel;
	uint32_t stale_us;
	int32_t scale_factor;
};

static void lq_hw_sensor_sample_work_handler(struct k_work *work)
{
	struct lq_hw_sensor_data *data = CONTAINER_OF(work, struct lq_hw_sensor_data, sample_work);
	
	int ret = sensor_sample_fetch(data->sensor_dev);
	if (ret == 0) {
		struct sensor_value val;
		ret = sensor_channel_get(data->sensor_dev, data->channel, &val);
		if (ret == 0) {
			/* Convert sensor_value to int32_t */
			int32_t value_int = val.val1 * data->scale_factor + 
			                    (val.val2 * data->scale_factor) / 1000000;
			lq_hw_push(data->hw_source, (uint32_t)value_int);
		}
	} else {
		LOG_ERR("Sensor fetch failed: %d", ret);
	}
}

static void lq_hw_sensor_timer_handler(struct k_timer *timer)
{
	struct lq_hw_sensor_data *data = CONTAINER_OF(timer, struct lq_hw_sensor_data, sample_timer);
	k_work_submit(&data->sample_work);
}

static int lq_hw_sensor_init(const struct device *dev)
{
	struct lq_hw_sensor_data *data = dev->data;
	const struct lq_hw_sensor_config *config = dev->config;
	
	if (!device_is_ready(config->sensor_dev)) {
		LOG_ERR("Sensor device not ready");
		return -ENODEV;
	}
	
	data->sensor_dev = config->sensor_dev;
	data->channel = config->channel;
	data->scale_factor = config->scale_factor;
	data->hw_source = LQ_HW_GPIO0 + DT_INST_NODE_IDX(dev); /* Reuse GPIO range */
	
	k_work_init(&data->sample_work, lq_hw_sensor_sample_work_handler);
	k_timer_init(&data->sample_timer, lq_hw_sensor_timer_handler, NULL);
	k_timer_start(&data->sample_timer, K_MSEC(100), K_MSEC(100));
	
	LOG_INF("Initialized sensor input ch%d -> hw_source %d", 
	        data->channel, data->hw_source);
	
	return 0;
}

#define LQ_HW_SENSOR_DEFINE(inst)					\
	static struct lq_hw_sensor_data lq_hw_sensor_data_##inst;	\
									\
	static const struct lq_hw_sensor_config lq_hw_sensor_config_##inst = { \
		.sensor_dev = DEVICE_DT_GET(DT_INST_PHANDLE(inst, sensor_device)), \
		.channel = DT_INST_PROP_OR(inst, sensor_channel, 0),	\
		.scale_factor = DT_INST_PROP_OR(inst, scale_factor, 100), \
		.stale_us = DT_INST_PROP_OR(inst, stale_us, 200000),	\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_hw_sensor_init,			\
			      NULL,					\
			      &lq_hw_sensor_data_##inst,		\
			      &lq_hw_sensor_config_##inst,		\
			      POST_KERNEL,				\
			      CONFIG_LQ_HW_SENSOR_INIT_PRIORITY,	\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_HW_SENSOR_DEFINE)
