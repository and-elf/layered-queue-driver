/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Driver for lq,system-health
 * 
 * Publishes system health status as a signal for monitoring by fault detectors.
 * Critical for safety systems - enables fail-safe modes when sensors fail.
 */

#define DT_DRV_COMPAT lq_system_health

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "lq_system_health.h"
#include "lq_hw_input.h"

LOG_MODULE_REGISTER(lq_system_health, CONFIG_LQ_LOG_LEVEL);

/* Device runtime data */
struct lq_system_health_data {
	enum lq_hw_source hw_source;
	struct k_work_delayable health_work;
	uint32_t update_period_ms;
};

/* Device configuration */
struct lq_system_health_config {
	uint32_t update_period_ms;
};

/* Periodic health status publisher */
static void lq_system_health_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct lq_system_health_data *data = CONTAINER_OF(dwork, 
	                                                    struct lq_system_health_data,
	                                                    health_work);
	
	/* Get current system health status */
	uint32_t health_signal = lq_health_as_signal();
	
	/* Publish to signal bus for monitoring */
	lq_hw_push(data->hw_source, health_signal);
	
	/* Log critical failures */
	if (!lq_health_is_system_safe()) {
		const struct lq_health_registry *status = lq_health_get_status();
		LOG_ERR("SYSTEM UNSAFE: %u critical failures detected!", 
		        status->critical_failures);
		
		/* Log each failed device */
		for (int i = 0; i < status->num_devices; i++) {
			const struct lq_health_entry *entry = &status->entries[i];
			if (entry->status == LQ_HEALTH_INIT_FAILED ||
			    entry->status == LQ_HEALTH_FAILED) {
				LOG_ERR("  - %s: status=%d, error=%d",
				        entry->device_name,
				        entry->status,
				        entry->error_code);
			}
		}
	}
	
	/* Reschedule */
	k_work_schedule(dwork, K_MSEC(data->update_period_ms));
}

static int lq_system_health_init(const struct device *dev)
{
	struct lq_system_health_data *data = dev->data;
	const struct lq_system_health_config *config = dev->config;
	
	/* Initialize health monitoring system */
	lq_health_init();
	
	/* Assign signal ID for health status */
	data->hw_source = LQ_HW_GPIO1;  /* Use GPIO range for system signals */
	data->update_period_ms = config->update_period_ms;
	
	/* Initialize periodic health status publisher */
	k_work_init_delayable(&data->health_work, lq_system_health_work_handler);
	k_work_schedule(&data->health_work, K_MSEC(data->update_period_ms));
	
	LOG_INF("System health monitor initialized (period=%u ms)", 
	        config->update_period_ms);
	
	return 0;
}

/* Device instantiation macro */
#define LQ_SYSTEM_HEALTH_DEFINE(inst)					\
	static struct lq_system_health_data lq_system_health_data_##inst; \
									\
	static const struct lq_system_health_config lq_system_health_config_##inst = { \
		.update_period_ms = DT_INST_PROP_OR(inst, update_period_ms, 100), \
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			      lq_system_health_init,			\
			      NULL,					\
			      &lq_system_health_data_##inst,		\
			      &lq_system_health_config_##inst,		\
			      POST_KERNEL,				\
			      CONFIG_LQ_SYSTEM_HEALTH_INIT_PRIORITY,	\
			      NULL);

/* Instantiate all instances from devicetree */
DT_INST_FOREACH_STATUS_OKAY(LQ_SYSTEM_HEALTH_DEFINE)
