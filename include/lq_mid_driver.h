/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mid-level driver interface
 * 
 * Pure, functional processing layer that transforms raw hardware
 * samples into validated events. No RTOS dependencies.
 */

#ifndef LQ_MID_DRIVER_H_
#define LQ_MID_DRIVER_H_

#include "lq_common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct lq_mid_driver;
struct lq_event;

/**
 * @brief Mid-level driver virtual table
 * 
 * Defines the interface for all mid-level drivers.
 * These drivers are pure - they take raw input and produce events,
 * with no side effects or RTOS calls.
 */
struct lq_mid_vtbl {
    /**
     * @brief Initialize driver state
     * 
     * @param drv Driver instance
     */
    void (*init)(struct lq_mid_driver *drv);
    
    /**
     * @brief Process raw input and generate events
     * 
     * This is a pure function - no RTOS calls, no side effects.
     * It processes raw hardware samples and produces validated events.
     * 
     * @param drv Driver instance
     * @param now Current timestamp (microseconds)
     * @param raw Raw input data (type depends on driver)
     * @param out_events Output buffer for generated events
     * @param max_events Maximum number of events that can be written
     * @return Number of events generated (0 to max_events)
     */
    size_t (*process)(
        struct lq_mid_driver *drv,
        uint64_t now,
        const void *raw,
        struct lq_event *out_events,
        size_t max_events);
    
    /**
     * @brief Get driver statistics (optional)
     * 
     * @param drv Driver instance
     * @param stats Output buffer for statistics
     */
    void (*get_stats)(struct lq_mid_driver *drv, void *stats);
};

/**
 * @brief Mid-level driver instance
 * 
 * Base structure for all mid-level drivers.
 * Concrete drivers embed this and add their own state.
 */
struct lq_mid_driver {
    const struct lq_mid_vtbl *v;  /**< Virtual table */
    void *ctx;                     /**< Driver-specific context */
};

/**
 * @brief Initialize a mid-level driver
 * 
 * @param drv Driver instance
 */
static inline void lq_mid_init(struct lq_mid_driver *drv)
{
    if (drv && drv->v && drv->v->init) {
        drv->v->init(drv);
    }
}

/**
 * @brief Process input through mid-level driver
 * 
 * @param drv Driver instance
 * @param now Current timestamp
 * @param raw Raw input data
 * @param out_events Output event buffer
 * @param max_events Maximum output events
 * @return Number of events generated
 */
static inline size_t lq_mid_process(
    struct lq_mid_driver *drv,
    uint64_t now,
    const void *raw,
    struct lq_event *out_events,
    size_t max_events)
{
    if (!drv || !drv->v || !drv->v->process) {
        return 0;
    }
    return drv->v->process(drv, now, raw, out_events, max_events);
}

/* ============================================================================
 * Common mid-level driver types
 * ============================================================================ */

/**
 * @brief ADC input driver context
 * 
 * Validates ADC samples against expected ranges.
 */
struct lq_mid_adc_ctx {
    uint32_t min_raw;          /**< Minimum valid raw value */
    uint32_t max_raw;          /**< Maximum valid raw value */
    uint64_t stale_us;         /**< Staleness timeout (microseconds) */
    uint64_t last_sample_ts;   /**< Last sample timestamp */
    uint32_t last_value;       /**< Last validated value */
};

/**
 * @brief SPI input driver context
 * 
 * Validates SPI reads and detects communication errors.
 */
struct lq_mid_spi_ctx {
    uint64_t stale_us;         /**< Staleness timeout (microseconds) */
    uint64_t last_sample_ts;   /**< Last sample timestamp */
    uint32_t last_value;       /**< Last validated value */
    uint32_t error_count;      /**< Communication error count */
};

/**
 * @brief Merge/voter driver context
 * 
 * Combines multiple inputs with voting algorithms.
 */
struct lq_mid_merge_ctx {
    const struct lq_mid_driver **inputs;  /**< Input drivers */
    size_t num_inputs;                     /**< Number of inputs */
    uint32_t tolerance;                    /**< Maximum deviation */
    uint64_t stale_us;                     /**< Staleness timeout */
    enum lq_vote_method voting_method;     /**< Voting algorithm */
};

#ifdef __cplusplus
}
#endif

#endif /* LQ_MID_DRIVER_H_ */
