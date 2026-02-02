/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,pid
 */

#define DT_DRV_COMPAT lq_pid

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "lq_pid.h"

LOG_MODULE_REGISTER(lq_pid, CONFIG_LQ_LOG_LEVEL);

/* Device configuration */
struct lq_pid_config {
	struct lq_pid_ctx ctx;
};

static int lq_pid_init(const struct device *dev)
{
	const struct lq_pid_config *config = dev->config;
	
	LOG_INF("Initialized PID controller: setpoint=%u, output=%u, kp=%d, ki=%d, kd=%d",
	        config->ctx.setpoint_signal, config->ctx.output_signal,
	        config->ctx.kp, config->ctx.ki, config->ctx.kd);
	
	return 0;
}

#define LQ_PID_SOURCE_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, process_variable), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, process_variable))), \
		(DT_INST_PROP_OR(inst, process_variable_signal_id, 0)))

#define LQ_PID_SETPOINT_SIGNAL(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, setpoint), \
		(DT_DEP_ORD(DT_INST_PHANDLE(inst, setpoint))), \
		(DT_INST_PROP_OR(inst, setpoint_signal_id, 0)))

#define LQ_PID_DEFINE(inst)						\
	static const struct lq_pid_config lq_pid_config_##inst = {	\
		.ctx = {						\
			.process_variable_signal = LQ_PID_SOURCE_SIGNAL(inst), \
			.setpoint_signal = LQ_PID_SETPOINT_SIGNAL(inst), \
			.output_signal = DT_DEP_ORD(DT_DRV_INST(inst)),	\
			.kp = DT_INST_PROP(inst, kp),			\
			.ki = DT_INST_PROP_OR(inst, ki, 0),		\
			.kd = DT_INST_PROP_OR(inst, kd, 0),		\
			.integral_min = DT_INST_PROP_OR(inst, integral_min, INT32_MIN), \
			.integral_max = DT_INST_PROP_OR(inst, integral_max, INT32_MAX), \
			.output_min = DT_INST_PROP_OR(inst, output_min, INT32_MIN), \
			.output_max = DT_INST_PROP_OR(inst, output_max, INT32_MAX), \
			.derivative_filter_alpha = DT_INST_PROP_OR(inst, derivative_filter_alpha, 1000), \
			.enabled = true,				\
		},							\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_pid_init,				\
			      NULL,					\
			      NULL,					\
			      &lq_pid_config_##inst,			\
			      POST_KERNEL,				\
			      CONFIG_LQ_PID_INIT_PRIORITY,		\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(LQ_PID_DEFINE)
