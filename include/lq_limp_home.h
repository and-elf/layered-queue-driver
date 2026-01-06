/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LQ_LIMP_HOME_H
#define LQ_LIMP_HOME_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct lq_engine;
struct lq_scale_ctx;

/**
 * @brief Limp-home mode controller context
 * 
 * Monitors fault signals and dynamically modifies scale driver parameters
 * to implement safety degradation when faults are detected.
 */
struct lq_limp_home_ctx {
    /** Signal ID to monitor for faults */
    uint16_t fault_signal_id;
    
    /** Fault threshold that triggers limp mode (default: 1) */
    int32_t fault_threshold;
    
    /** Index of scale driver to modify */
    uint8_t target_scale_id;
    
    /** Limp mode parameters (INT32_MIN = not overridden) */
    int32_t limp_scale_factor;
    int32_t limp_clamp_max;
    int32_t limp_clamp_min;
    
    /** Restore delay in milliseconds (0 = immediate) */
    uint32_t restore_delay_ms;
    
    /** Current state */
    bool is_limp_mode_active;
    uint64_t fault_clear_timestamp_ms;
    
    /** Saved normal mode parameters (to restore when fault clears) */
    int32_t saved_scale_factor;
    int32_t saved_clamp_max;
    int32_t saved_clamp_min;
    bool saved_has_clamp;
    
    /** Whether this controller is enabled */
    bool enabled;
};

/**
 * @brief Process all limp-home mode controllers
 * 
 * Monitors fault signals and modifies scale parameters when faults are detected.
 * Should be called after fault monitoring but before scale processing.
 * 
 * @param engine Engine context containing signals and scale drivers
 */
void lq_process_limp_home(struct lq_engine *engine);

#endif /* LQ_LIMP_HOME_H */
