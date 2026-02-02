/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,remap
 */

#define DT_DRV_COMPAT lq_remap

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "lq_remap.h"

LOG_MODULE_REGISTER(lq_remap, CONFIG_LQ_LOG_LEVEL);

struct lq_remap_config {
	struct lq_remap_ctx ctx;
};

static int lq_remap_init(const struct device *dev)
{
	const struct lq_remap_config *config = dev->config;
	
	LOG_INF("Initialized remap driver: signal %u -> %u",
	        config->ctx.input_signal, config->ctx.output_signal);
	
	return 0;
}

#define LQ_REMAP_SOURCE_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, source), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, source))), \
		(DT_INST_PROP_OR(inst, input_signal_id, 0)))

#define LQ_REMAP_DEFINE(inst)						\
	static const struct lq_remap_config lq_remap_config_##inst = {	\
		.ctx = {						\
			.input_signal = LQ_REMAP_SOURCE_SIGNAL(inst),	\
			.output_signal = DT_DEP_ORD(DT_DRV_INST(inst)),	\
			.input_min = DT_INST_PROP(inst, input_min),	\
			.input_max = DT_INST_PROP(inst, input_max),	\
			.output_min = DT_INST_PROP(inst, output_min),	\
			.output_max = DT_INST_PROP(inst, output_max),	\
			.clamp_output = DT_INST_PROP_OR(inst, clamp_output, true), \
			.enabled = true,				\
		},							\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_remap_init,				\
			      NULL,					\
			      NULL,					\
			      &lq_remap_config_##inst,			\
			      POST_KERNEL,				\
			      CONFIG_LQ_REMAP_INIT_PRIORITY,		\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_REMAP_DEFINE)
