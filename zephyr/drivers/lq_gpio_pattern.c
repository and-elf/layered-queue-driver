/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,gpio-pattern
 */

#define DT_DRV_COMPAT lq_gpio_pattern

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lq_gpio_pattern, CONFIG_LQ_LOG_LEVEL);

struct lq_gpio_pattern_config {
	uint8_t input_signal;
	struct gpio_dt_spec gpio;
	uint32_t pattern_id;
};

static int lq_gpio_pattern_init(const struct device *dev)
{
	const struct lq_gpio_pattern_config *config = dev->config;
	
	if (!gpio_is_ready_dt(&config->gpio)) {
		LOG_ERR("GPIO device not ready");
		return -ENODEV;
	}
	
	int ret = gpio_pin_configure_dt(&config->gpio, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("GPIO configure failed: %d", ret);
		return ret;
	}
	
	LOG_INF("Initialized GPIO pattern: signal=%u, pattern=%u",
	        config->input_signal, config->pattern_id);
	
	return 0;
}

#define LQ_GPIO_SOURCE_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, input), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, input))), \
		(DT_INST_PROP_OR(inst, input_signal_id, 0)))

#define LQ_GPIO_PATTERN_DEFINE(inst)					\
	static const struct lq_gpio_pattern_config lq_gpio_pattern_config_##inst = { \
		.input_signal = LQ_GPIO_SOURCE_SIGNAL(inst),		\
		.gpio = GPIO_DT_SPEC_INST_GET(inst, gpios),		\
		.pattern_id = DT_INST_PROP_OR(inst, pattern_id, 0),	\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_gpio_pattern_init,			\
			      NULL,					\
			      NULL,					\
			      &lq_gpio_pattern_config_##inst,		\
			      POST_KERNEL,				\
			      CONFIG_LQ_GPIO_PATTERN_INIT_PRIORITY,	\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_GPIO_PATTERN_DEFINE)
