/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,hw-adc-input
 */

#define DT_DRV_COMPAT lq_hw_adc_input

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

#include "lq_hw_input.h"
#include "lq_platform.h"

LOG_MODULE_REGISTER(lq_hw_adc, CONFIG_LQ_LOG_LEVEL);

/* Device runtime data */
struct lq_hw_adc_data {
	const struct device *adc_dev;
	struct adc_channel_cfg channel_cfg;
	struct adc_sequence sequence;
	int16_t sample_buffer;
	enum lq_hw_source hw_source;
	uint32_t last_value;
};

/* Device configuration (from devicetree) */
struct lq_hw_adc_config {
	const struct device *adc_dev;
	uint8_t channel_id;
	uint8_t isr_priority;
	int32_t min_raw;
	int32_t max_raw;
	uint32_t stale_us;
};

/* ADC sampling work handler */
static void lq_hw_adc_sample_work_handler(struct k_work *work)
{
	struct lq_hw_adc_data *data = CONTAINER_OF(work, struct lq_hw_adc_data, sample_work);
	
	int ret = adc_read(data->adc_dev, &data->sequence);
	if (ret == 0) {
		uint32_t value = (uint32_t)data->sample_buffer;
		lq_hw_push(data->hw_source, value);
		data->last_value = value;
	} else {
		LOG_ERR("ADC read failed: %d", ret);
	}
}

/* Timer handler for periodic sampling */
static void lq_hw_adc_timer_handler(struct k_timer *timer)
{
	struct lq_hw_adc_data *data = CONTAINER_OF(timer, struct lq_hw_adc_data, sample_timer);
	k_work_submit(&data->sample_work);
}

/* Device initialization */
static int lq_hw_adc_init(const struct device *dev)
{
	struct lq_hw_adc_data *data = dev->data;
	const struct lq_hw_adc_config *config = dev->config;
	
	/* Verify ADC device is ready */
	if (!device_is_ready(config->adc_dev)) {
		LOG_ERR("ADC device not ready");
		return -ENODEV;
	}
	
	data->adc_dev = config->adc_dev;
	
	/* Configure ADC channel */
	data->channel_cfg.gain = ADC_GAIN_1;
	data->channel_cfg.reference = ADC_REF_INTERNAL;
	data->channel_cfg.acquisition_time = ADC_ACQ_TIME_DEFAULT;
	data->channel_cfg.channel_id = config->channel_id;
	
	int ret = adc_channel_setup(data->adc_dev, &data->channel_cfg);
	if (ret < 0) {
		LOG_ERR("ADC channel setup failed: %d", ret);
		return ret;
	}
	
	/* Configure ADC sequence */
	data->sequence.channels = BIT(config->channel_id);
	data->sequence.buffer = &data->sample_buffer;
	data->sequence.buffer_size = sizeof(data->sample_buffer);
	data->sequence.resolution = 12; /* Common default */
	
	/* Assign hardware source ID based on device ordinal */
	data->hw_source = LQ_HW_ADC0 + DT_INST_NODE_IDX(dev);
	
	/* Initialize work queue for sampling */
	k_work_init(&data->sample_work, lq_hw_adc_sample_work_handler);
	
	/* Start periodic sampling timer (every 10ms by default) */
	k_timer_init(&data->sample_timer, lq_hw_adc_timer_handler, NULL);
	k_timer_start(&data->sample_timer, K_MSEC(10), K_MSEC(10));
	
	LOG_INF("Initialized ADC input ch%u -> hw_source %d", 
	        config->channel_id, data->hw_source);
	
	return 0;
}

/* Device instantiation macro */
#define LQ_HW_ADC_DEFINE(inst)						\
	static struct lq_hw_adc_data lq_hw_adc_data_##inst = {		\
		.sample_work = Z_WORK_INITIALIZER(lq_hw_adc_sample_work_handler), \
	};								\
									\
	static const struct lq_hw_adc_config lq_hw_adc_config_##inst = { \
		.adc_dev = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(DT_DRV_INST(inst))), \
		.channel_id = DT_IO_CHANNELS_INPUT(DT_DRV_INST(inst)),	\
		.isr_priority = DT_INST_PROP_OR(inst, isr_priority, 5), \
		.min_raw = DT_INST_PROP_OR(inst, min_raw, 0),		\
		.max_raw = DT_INST_PROP_OR(inst, max_raw, 4095),	\
		.stale_us = DT_INST_PROP_OR(inst, stale_us, 100000),	\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_hw_adc_init,				\
			      NULL,					\
			      &lq_hw_adc_data_##inst,			\
			      &lq_hw_adc_config_##inst,			\
			      POST_KERNEL,				\
			      CONFIG_LQ_HW_ADC_INIT_PRIORITY,		\
			      NULL);

/* Instantiate all instances from devicetree */
DT_INST_FOREACH_STATUS_OKAY(LQ_HW_ADC_DEFINE)
