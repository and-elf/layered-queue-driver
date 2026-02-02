/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,fault-monitor
 */

#define DT_DRV_COMPAT lq_fault_monitor

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "lq_event.h"

LOG_MODULE_REGISTER(lq_fault_monitor, CONFIG_LQ_LOG_LEVEL);

struct lq_fault_monitor_config {
	struct lq_fault_monitor_ctx ctx;
};

static int lq_fault_monitor_init(const struct device *dev)
{
	const struct lq_fault_monitor_config *config = dev->config;
	
	LOG_INF("Initialized fault monitor: input=%u, output=%u, thresholds=[%d, %d]",
	        config->ctx.input_signal, config->ctx.output_signal,
	        config->ctx.threshold_low, config->ctx.threshold_high);
	
	return 0;
}

#define LQ_FAULT_SOURCE_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, input), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, input))), \
		(DT_INST_PROP_OR(inst, input_signal_id, 0)))

#define LQ_FAULT_MONITOR_DEFINE(inst)					\
	static const struct lq_fault_monitor_config lq_fault_monitor_config_##inst = { \
		.ctx = {						\
			.input_signal = LQ_FAULT_SOURCE_SIGNAL(inst),	\
			.output_signal = DT_DEP_ORD(DT_DRV_INST(inst)),	\
			.threshold_low = DT_INST_PROP_OR(inst, threshold_low, INT32_MIN), \
			.threshold_high = DT_INST_PROP_OR(inst, threshold_high, INT32_MAX), \
			.hysteresis = DT_INST_PROP_OR(inst, hysteresis, 0), \
			.debounce_ms = DT_INST_PROP_OR(inst, debounce_ms, 0), \
			.enabled = true,				\
		},							\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_fault_monitor_init,			\
			      NULL,					\
			      NULL,					\
			      &lq_fault_monitor_config_##inst,		\
			      POST_KERNEL,				\
			      CONFIG_LQ_FAULT_MONITOR_INIT_PRIORITY,	\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_FAULT_MONITOR_DEFINE)
