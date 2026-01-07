/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Configuration Framework
 * 
 * Provides runtime configuration access via UDS DIDs.
 * Connects diagnostic services to remap/scale parameters,
 * signal values, and system status.
 */

#ifndef LQ_CONFIG_H_
#define LQ_CONFIG_H_

#include "lq_engine.h"
#include "lq_remap.h"
#include "lq_scale.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration Registry
 * ============================================================================ */

/**
 * @brief Maximum number of remap configurations
 */
#ifndef LQ_MAX_REMAP_CONFIGS
#define LQ_MAX_REMAP_CONFIGS 16
#endif

/**
 * @brief Maximum number of scale configurations
 */
#ifndef LQ_MAX_SCALE_CONFIGS
#define LQ_MAX_SCALE_CONFIGS 16
#endif

/**
 * @brief Configuration registry
 * 
 * Central registry for all configurable parameters.
 * Used by UDS DID handlers to access configuration data.
 */
struct lq_config_registry {
    /* Engine reference */
    struct lq_engine *engine;
    
    /* Remap configurations */
    struct lq_remap_ctx *remaps;
    uint8_t num_remaps;
    uint8_t max_remaps;
    
    /* Scale configurations */
    struct lq_scale_ctx *scales;
    uint8_t num_scales;
    uint8_t max_scales;
    
    /* Configuration lock state */
    bool config_locked;         /* Prevent modifications in production */
    bool calibration_mode;      /* Calibration mode active */
    
    /* Change tracking */
    uint32_t config_version;    /* Increment on each change */
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Initialize configuration registry
 * 
 * @param registry Configuration registry
 * @param engine Engine instance
 * @param remaps Remap configuration array
 * @param max_remaps Maximum number of remaps
 * @param scales Scale configuration array
 * @param max_scales Maximum number of scales
 * @return 0 on success, negative errno on failure
 */
int lq_config_init(struct lq_config_registry *registry,
                   struct lq_engine *engine,
                   struct lq_remap_ctx *remaps, uint8_t max_remaps,
                   struct lq_scale_ctx *scales, uint8_t max_scales);

/**
 * @brief Read signal value by ID
 * 
 * @param registry Configuration registry
 * @param signal_id Signal ID
 * @param value Output: signal value
 * @param status Output: signal status
 * @return 0 on success, negative errno on failure
 */
int lq_config_read_signal(const struct lq_config_registry *registry,
                          uint8_t signal_id,
                          int32_t *value,
                          uint8_t *status);

/**
 * @brief Read remap configuration by index
 * 
 * @param registry Configuration registry
 * @param index Remap index (0-based)
 * @param remap Output: remap configuration
 * @return 0 on success, negative errno on failure
 */
int lq_config_read_remap(const struct lq_config_registry *registry,
                         uint8_t index,
                         struct lq_remap_ctx *remap);

/**
 * @brief Write remap configuration by index
 * 
 * @param registry Configuration registry
 * @param index Remap index
 * @param remap New remap configuration
 * @return 0 on success, negative errno on failure
 */
int lq_config_write_remap(struct lq_config_registry *registry,
                          uint8_t index,
                          const struct lq_remap_ctx *remap);

/**
 * @brief Read scale configuration by index
 * 
 * @param registry Configuration registry
 * @param index Scale index (0-based)
 * @param scale Output: scale configuration
 * @return 0 on success, negative errno on failure
 */
int lq_config_read_scale(const struct lq_config_registry *registry,
                         uint8_t index,
                         struct lq_scale_ctx *scale);

/**
 * @brief Write scale configuration by index
 * 
 * @param registry Configuration registry
 * @param index Scale index
 * @param scale New scale configuration
 * @return 0 on success, negative errno on failure
 */
int lq_config_write_scale(struct lq_config_registry *registry,
                          uint8_t index,
                          const struct lq_scale_ctx *scale);

/**
 * @brief Enter calibration mode
 * 
 * Allows configuration changes. Requires security access in UDS context.
 * 
 * @param registry Configuration registry
 * @return 0 on success, negative errno on failure
 */
int lq_config_enter_calibration_mode(struct lq_config_registry *registry);

/**
 * @brief Exit calibration mode
 * 
 * Validates and locks configuration.
 * 
 * @param registry Configuration registry
 * @return 0 on success, negative errno on failure
 */
int lq_config_exit_calibration_mode(struct lq_config_registry *registry);

/**
 * @brief Reset all configurations to defaults
 * 
 * @param registry Configuration registry
 * @return 0 on success, negative errno on failure
 */
int lq_config_reset_to_defaults(struct lq_config_registry *registry);

/**
 * @brief Add a new remap configuration
 * 
 * @param registry Configuration registry
 * @param remap Remap to add
 * @param index Output: index of added remap
 * @return 0 on success, negative errno on failure
 */
int lq_config_add_remap(struct lq_config_registry *registry,
                        const struct lq_remap_ctx *remap,
                        uint8_t *index);

/**
 * @brief Add a new scale configuration
 * 
 * @param registry Configuration registry
 * @param scale Scale to add
 * @param index Output: index of added scale
 * @return 0 on success, negative errno on failure
 */
int lq_config_add_scale(struct lq_config_registry *registry,
                        const struct lq_scale_ctx *scale,
                        uint8_t *index);

/**
 * @brief Remove remap configuration by index
 * 
 * @param registry Configuration registry
 * @param index Remap index to remove
 * @return 0 on success, negative errno on failure
 */
int lq_config_remove_remap(struct lq_config_registry *registry,
                           uint8_t index);

/**
 * @brief Remove scale configuration by index
 * 
 * @param registry Configuration registry
 * @param index Scale index to remove
 * @return 0 on success, negative errno on failure
 */
int lq_config_remove_scale(struct lq_config_registry *registry,
                           uint8_t index);

/* ============================================================================
 * UDS DID Handlers
 * ============================================================================ */

/**
 * @brief UDS Read DID handler for configuration framework
 * 
 * Supports DIDs:
 * - 0xF1A0: Read signal value (param: signal_id)
 * - 0xF1A1: Read signal status (param: signal_id)
 * - 0xF1A2: Read remap config (param: remap_index)
 * - 0xF1A3: Read scale config (param: scale_index)
 * - 0xF1A4: Read calibration mode status
 * 
 * @param registry Configuration registry
 * @param did Data identifier
 * @param data Output buffer
 * @param max_len Maximum output length
 * @param actual_len Output: actual length written
 * @return 0 on success, negative UDS NRC on error
 */
int lq_config_uds_read_did(const struct lq_config_registry *registry,
                           uint16_t did,
                           uint8_t *data,
                           size_t max_len,
                           size_t *actual_len);

/**
 * @brief UDS Write DID handler for configuration framework
 * 
 * Supports DIDs:
 * - 0xF1A2: Write remap config (format: index + remap_ctx)
 * - 0xF1A3: Write scale config (format: index + scale_ctx)
 * 
 * @param registry Configuration registry
 * @param did Data identifier
 * @param data Input data
 * @param len Input length
 * @return 0 on success, negative UDS NRC on error
 */
int lq_config_uds_write_did(struct lq_config_registry *registry,
                            uint16_t did,
                            const uint8_t *data,
                            size_t len);

/**
 * @brief UDS Routine Control handler for configuration framework
 * 
 * Supports routines:
 * - 0xF1A0: Enter calibration mode
 * - 0xF1A1: Exit calibration mode
 * - 0xF1A2: Reset to defaults
 * 
 * @param registry Configuration registry
 * @param rid Routine identifier
 * @param control_type Start/Stop/Request Results
 * @param in_data Input parameters
 * @param in_len Input length
 * @param out_data Output buffer
 * @param max_out Maximum output length
 * @param actual_out Output: actual length written
 * @return 0 on success, negative UDS NRC on error
 */
int lq_config_uds_routine_control(struct lq_config_registry *registry,
                                  uint16_t rid,
                                  uint8_t control_type,
                                  const uint8_t *in_data,
                                  size_t in_len,
                                  uint8_t *out_data,
                                  size_t max_out,
                                  size_t *actual_out);

#ifdef __cplusplus
}
#endif

#endif /* LQ_CONFIG_H_ */
