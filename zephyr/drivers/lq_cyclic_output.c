/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,cyclic-output
 */

#define DT_DRV_COMPAT lq_cyclic_output

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lq_cyclic_output, CONFIG_LQ_LOG_LEVEL);

/* Device configuration */
struct lq_cyclic_output_config {
	uint8_t source_signal;
	const char *output_type;
	uint32_t target_id;
	uint32_t period_ms;
};

static int lq_cyclic_output_init(const struct device *dev)
{
	const struct lq_cyclic_output_config *config = dev->config;
	
	LOG_INF("Initialized cyclic output: signal=%u, type=%s, target=0x%x, period=%u ms",
	        config->source_signal, config->output_type, 
	        config->target_id, config->period_ms);
	
	return 0;
}

#define LQ_CYCLIC_SOURCE_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, source), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, source))), \
		(DT_INST_PROP_OR(inst, source_signal_id, 0)))

#define LQ_CYCLIC_OUTPUT_DEFINE(inst)					\
	static const struct lq_cyclic_output_config lq_cyclic_output_config_##inst = { \
		.source_signal = LQ_CYCLIC_SOURCE_SIGNAL(inst),		\
		.output_type = DT_INST_PROP(inst, output_type),		\
		.target_id = DT_INST_PROP_OR(inst, target_id, 0),	\
		.period_ms = DT_INST_PROP_OR(inst, period_ms, 100),	\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_cyclic_output_init,			\
			      NULL,					\
			      NULL,					\
			      &lq_cyclic_output_config_##inst,		\
			      POST_KERNEL,				\
			      CONFIG_LQ_CYCLIC_OUTPUT_INIT_PRIORITY,	\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_CYCLIC_OUTPUT_DEFINE)
