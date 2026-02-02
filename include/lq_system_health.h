/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * System Health Monitoring for Safety-Critical Systems
 * 
 * Tracks initialization and runtime health of all hardware peripherals.
 * Critical for fail-safe operation - must know when sensors are unavailable.
 */

#ifndef LQ_SYSTEM_HEALTH_H_
#define LQ_SYSTEM_HEALTH_H_

#include <stdint.h>
#include <stdbool.h>
#include "lq_config.h"  /* For LQ_MAX_HEALTH_DEVICES (auto-generated) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware health status codes
 */
enum lq_health_status {
	LQ_HEALTH_UNKNOWN = 0,      /**< Not yet initialized */
	LQ_HEALTH_OK = 1,           /**< Device ready and operational */
	LQ_HEALTH_INIT_FAILED = 2,  /**< Initialization failed - CRITICAL */
	LQ_HEALTH_DEGRADED = 3,     /**< Working but with errors */
	LQ_HEALTH_STALE = 4,        /**< No new data within timeout */
	LQ_HEALTH_FAILED = 5,       /**< Runtime failure detected */
};

/**
 * @brief Health status entry for a hardware device
 */
struct lq_health_entry {
	const char *device_name;        /**< Device name for logging */
	enum lq_health_status status;   /**< Current health status */
	uint32_t error_code;            /**< Last error code (errno) */
	uint64_t last_update_us;        /**< Timestamp of last health update */
	uint32_t consecutive_errors;    /**< Count of consecutive errors */
};

/**
 * @brief System-wide health registry
 * 
 * All hardware input drivers should register here during initialization
 * and update their status during runtime.
 */
struct lq_health_registry {
	struct lq_health_entry entries[LQ_MAX_HEALTH_DEVICES];
	uint8_t num_devices;
	uint8_t critical_failures;  /**< Count of INIT_FAILED or FAILED devices */
	bool system_safe;           /**< False if any critical device failed */
};

/**
 * @brief Register a hardware device with the health monitor
 * 
 * Call this during device initialization. If init fails, register with
 * status = LQ_HEALTH_INIT_FAILED before returning error.
 * 
 * @param name Device name for diagnostics
 * @param status Initial health status
 * @param error_code Error code (errno) if status indicates failure
 * @return Index in health registry, or -1 if registry full
 */
int lq_health_register(const char *name, 
                        enum lq_health_status status,
                        int error_code);

/**
 * @brief Update health status of a registered device
 * 
 * Call this:
 * - When device transitions to operational state
 * - When errors are detected
 * - Periodically to confirm device is still responsive
 * 
 * @param index Health registry index (from lq_health_register)
 * @param status New health status
 * @param error_code Error code if status indicates failure
 */
void lq_health_update(int index, 
                       enum lq_health_status status,
                       int error_code);

/**
 * @brief Check if system is safe to operate
 * 
 * Returns false if ANY device has status:
 * - LQ_HEALTH_INIT_FAILED
 * - LQ_HEALTH_FAILED
 * 
 * Safety-critical code should check this before enabling actuators.
 * 
 * @return true if all devices healthy, false if any critical failure
 */
bool lq_health_is_system_safe(void);

/**
 * @brief Get detailed health status
 * 
 * For diagnostic logging and fault analysis.
 * 
 * @return Pointer to health registry (read-only)
 */
const struct lq_health_registry* lq_health_get_status(void);

/**
 * @brief Get health status as a signal value
 * 
 * This can be used to publish system health to the signal bus
 * for monitoring by fault detectors.
 * 
 * Encoding:
 * - bit 0-7: Number of devices
 * - bit 8-15: Number of critical failures
 * - bit 16: System safe flag (1=safe, 0=UNSAFE)
 * 
 * @return Packed health status value
 */
uint32_t lq_health_as_signal(void);

/**
 * @brief Initialize health monitoring system
 * 
 * Must be called early in boot, before any device initialization.
 */
void lq_health_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LQ_SYSTEM_HEALTH_H_ */
