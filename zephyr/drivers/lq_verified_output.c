/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,verified-output
 */

#define DT_DRV_COMPAT lq_verified_output

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "lq_verified_output.h"

LOG_MODULE_REGISTER(lq_verified_output, CONFIG_LQ_LOG_LEVEL);

struct lq_verified_output_config {
	struct lq_verified_output_ctx ctx;
};

static int lq_verified_output_init(const struct device *dev)
{
	const struct lq_verified_output_config *config = dev->config;
	
	LOG_INF("Initialized verified output: primary=%u, secondary=%u",
	        config->ctx.primary_signal, config->ctx.secondary_signal);
	
	return 0;
}

#define LQ_VERIFIED_PRIMARY_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, primary), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, primary))), \
		(DT_INST_PROP_OR(inst, primary_signal_id, 0)))

#define LQ_VERIFIED_SECONDARY_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, secondary), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, secondary))), \
		(DT_INST_PROP_OR(inst, secondary_signal_id, 0)))

#define LQ_VERIFIED_OUTPUT_DEFINE(inst)					\
	static const struct lq_verified_output_config lq_verified_output_config_##inst = { \
		.ctx = {						\
			.primary_signal = LQ_VERIFIED_PRIMARY_SIGNAL(inst), \
			.secondary_signal = LQ_VERIFIED_SECONDARY_SIGNAL(inst), \
			.tolerance = DT_INST_PROP_OR(inst, tolerance, 100), \
			.enabled = true,				\
		},							\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_verified_output_init,			\
			      NULL,					\
			      NULL,					\
			      &lq_verified_output_config_##inst,	\
			      POST_KERNEL,				\
			      CONFIG_LQ_VERIFIED_OUTPUT_INIT_PRIORITY,	\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_VERIFIED_OUTPUT_DEFINE)
