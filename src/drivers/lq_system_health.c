/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * System Health Monitoring Implementation
 */

#include "lq_system_health.h"
#include "lq_platform.h"
#include <string.h>

/* Global health registry */
static struct lq_health_registry g_health_registry = {
	.num_devices = 0,
	.critical_failures = 0,
	.system_safe = true,
};

void lq_health_init(void)
{
	memset(&g_health_registry, 0, sizeof(g_health_registry));
	g_health_registry.system_safe = true;
}

int lq_health_register(const char *name, 
                        enum lq_health_status status,
                        int error_code)
{
	if (g_health_registry.num_devices >= 32) {
		return -1;  /* Registry full */
	}
	
	int index = g_health_registry.num_devices++;
	struct lq_health_entry *entry = &g_health_registry.entries[index];
	
	entry->device_name = name;
	entry->status = status;
	entry->error_code = error_code;
	entry->last_update_us = lq_platform_get_time_us();
	entry->consecutive_errors = 0;
	
	/* Track critical failures */
	if (status == LQ_HEALTH_INIT_FAILED || status == LQ_HEALTH_FAILED) {
		g_health_registry.critical_failures++;
		g_health_registry.system_safe = false;
	}
	
	return index;
}

void lq_health_update(int index, 
                       enum lq_health_status status,
                       int error_code)
{
	if (index < 0 || index >= g_health_registry.num_devices) {
		return;
	}
	
	struct lq_health_entry *entry = &g_health_registry.entries[index];
	enum lq_health_status old_status = entry->status;
	
	entry->status = status;
	entry->error_code = error_code;
	entry->last_update_us = lq_platform_get_time_us();
	
	/* Update error counters */
	if (status != LQ_HEALTH_OK) {
		entry->consecutive_errors++;
	} else {
		entry->consecutive_errors = 0;
	}
	
	/* Update critical failure count */
	bool was_critical = (old_status == LQ_HEALTH_INIT_FAILED || 
	                     old_status == LQ_HEALTH_FAILED);
	bool is_critical = (status == LQ_HEALTH_INIT_FAILED || 
	                    status == LQ_HEALTH_FAILED);
	
	if (!was_critical && is_critical) {
		g_health_registry.critical_failures++;
	} else if (was_critical && !is_critical) {
		g_health_registry.critical_failures--;
	}
	
	/* Update system safe flag */
	g_health_registry.system_safe = (g_health_registry.critical_failures == 0);
}

bool lq_health_is_system_safe(void)
{
	return g_health_registry.system_safe;
}

const struct lq_health_registry* lq_health_get_status(void)
{
	return &g_health_registry;
}

uint32_t lq_health_as_signal(void)
{
	uint32_t signal = 0;
	signal |= (g_health_registry.num_devices & 0xFF);
	signal |= ((g_health_registry.critical_failures & 0xFF) << 8);
	signal |= (g_health_registry.system_safe ? (1 << 16) : 0);
	return signal;
}
