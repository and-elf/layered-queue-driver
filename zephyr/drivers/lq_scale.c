/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,scale
 */

#define DT_DRV_COMPAT lq_scale

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "lq_scale.h"
#include "lq_engine.h"

LOG_MODULE_REGISTER(lq_scale, CONFIG_LQ_LOG_LEVEL);

/* Device configuration (from devicetree) */
struct lq_scale_config {
	struct lq_scale_ctx ctx;
};

/* Device initialization */
static int lq_scale_init(const struct device *dev)
{
	const struct lq_scale_config *config = dev->config;
	
	/* Register with engine (engine must handle registration) */
	LOG_INF("Initialized scale driver: signal %u -> %u, scale=%d, offset=%d",
	        config->ctx.input_signal, config->ctx.output_signal,
	        config->ctx.scale_factor, config->ctx.offset);
	
	return 0;
}

/* Helper to get source signal ID from phandle */
#define LQ_SCALE_SOURCE_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, source), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, source))), \
		(COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, input), \
			(DT_DEP_ORD(DT_INST_PHANDLE(inst, input))), \
			(DT_INST_PROP_OR(inst, input_signal_id, 0)))))

/* Device instantiation macro */
#define LQ_SCALE_DEFINE(inst)						\
	static const struct lq_scale_config lq_scale_config_##inst = {	\
		.ctx = {						\
			.input_signal = LQ_SCALE_SOURCE_SIGNAL(inst),	\
			.output_signal = DT_DEP_ORD(DT_DRV_INST(inst)),	\
			.scale_factor = DT_INST_PROP(inst, scale_factor), \
			.offset = DT_INST_PROP_OR(inst, offset, 0),	\
			.clamp_min = DT_INST_PROP_OR(inst, clamp_min, INT32_MIN), \
			.clamp_max = DT_INST_PROP_OR(inst, clamp_max, INT32_MAX), \
			.has_clamp_min = DT_INST_NODE_HAS_PROP(inst, clamp_min), \
			.has_clamp_max = DT_INST_NODE_HAS_PROP(inst, clamp_max), \
			.enabled = true,				\
		},							\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_scale_init,				\
			      NULL,					\
			      NULL,					\
			      &lq_scale_config_##inst,			\
			      POST_KERNEL,				\
			      CONFIG_LQ_SCALE_INIT_PRIORITY,		\
			      NULL);

/* Instantiate all instances from devicetree */
DT_INST_FOREACH_STATUS_OKAY(LQ_SCALE_DEFINE)
