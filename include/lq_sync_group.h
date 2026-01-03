/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Synchronized output groups
 * 
 * Coordinates multiple output drivers to ensure atomic,
 * time-synchronized updates across different hardware interfaces.
 */

#ifndef LQ_SYNC_GROUP_H_
#define LQ_SYNC_GROUP_H_

#include "lq_event.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Engine signal
 * 
 * Represents a processed signal value from a mid-level driver.
 * These are the canonical values maintained by the engine.
 */
struct lq_signal {
    int32_t value;                /**< Current signal value */
    enum lq_event_status status;  /**< Signal status */
    uint64_t timestamp;           /**< Last update timestamp */
    bool updated;                 /**< Set true when value changes */
};

/**
 * @brief Cyclic output context
 * 
 * Schedules periodic transmission of a signal to a specific output.
 * Uses deadline-based scheduling to ensure consistent timing.
 * 
 * Example: Send engine RPM on J1939 PGN 0xFEF1 every 100ms
 */
struct lq_cyclic_ctx {
    enum lq_output_type type;     /**< CAN / J1939 / CANopen / etc */
    uint32_t target_id;           /**< PGN, COB-ID, or other protocol ID */
    
    uint8_t source_signal;        /**< Index into engine signals array */
    
    uint64_t period_us;           /**< Transmission period (microseconds) */
    uint64_t next_deadline;       /**< Next scheduled transmission time */
    
    uint32_t flags;               /**< Protocol-specific flags */
};

/**
 * @brief Synchronized output group
 * 
 * Groups multiple output drivers together so they can be updated
 * atomically at a specific period. Useful for ensuring consistent
 * snapshots across CAN, GPIO, and other outputs.
 * 
 * Example: Update CAN frame and warning LED simultaneously every 100ms
 */
struct lq_sync_group {
    const char *name;                      /**< Group name for debugging */
    struct lq_output_driver **members;     /**< Array of output drivers */
    size_t num_members;                    /**< Number of members */
    uint64_t period_us;                    /**< Update period (microseconds) */
    uint64_t last_update_ts;               /**< Last update timestamp */
    bool use_staging;                      /**< Use stage/commit pattern */
};

/**
 * @brief Initialize synchronized output group
 * 
 * @param group Group instance
 * @return 0 on success, negative errno on failure
 */
int lq_sync_group_init(struct lq_sync_group *group);

/**
 * @brief Update synchronized group
 * 
 * Evaluates all member outputs and updates them atomically if:
 * 1. Sufficient time has elapsed (based on period_us)
 * 2. At least one source has new data
 * 
 * If use_staging is true, uses stage/commit pattern for atomicity.
 * Otherwise, writes directly to all outputs.
 * 
 * @param group Group instance
 * @param now Current timestamp (microseconds)
 * @param events Array of events to consider
 * @param num_events Number of events
 * @return 0 on success, negative errno on failure
 */
int lq_sync_group_update(struct lq_sync_group *group,
                         uint64_t now,
                         const struct lq_event *events,
                         size_t num_events);

/**
 * @brief Check if group is due for update
 * 
 * @param group Group instance
 * @param now Current timestamp
 * @return true if update period has elapsed
 */
static inline bool lq_sync_group_is_due(const struct lq_sync_group *group,
                                         uint64_t now)
{
    if (!group || group->period_us == 0) {
        return false;
    }
    return (now - group->last_update_ts) >= group->period_us;
}

/* ============================================================================
 * Cyclic output scheduling
 * ============================================================================ */

/**
 * @brief Process cyclic output context
 * 
 * Checks if output is due and generates an output event if deadline reached.
 * Uses deadline scheduling to avoid drift.
 * 
 * @param e Engine instance containing signals and output event buffer
 * @param c Cyclic context to process
 * @param now Current timestamp (microseconds)
 */
static inline void lq_cyclic_process(
    struct lq_engine *e,
    struct lq_cyclic_ctx *c,
    uint64_t now)
{
    if (now < c->next_deadline)
        return;
    
    const struct lq_signal *sig =
        &e->signals[c->source_signal];
    
    struct lq_output_event *evt =
        &e->out_events[e->out_event_count++];
    
    evt->type = c->type;
    evt->target_id = c->target_id;
    evt->value = sig->value;
    evt->flags = c->flags;
    evt->timestamp = now;
    
    c->next_deadline += c->period_us;
}

/* ============================================================================
 * Engine integration
 * ============================================================================ */

/**
 * @brief Engine configuration
 * 
 * Top-level structure that ties together all mid-level drivers,
 * output drivers, sync groups, signals, and cyclic contexts.
 */
struct lq_engine {
    struct lq_mid_driver **mid_drivers;    /**< Mid-level processors */
    size_t num_mid_drivers;
    
    struct lq_output_driver **outputs;     /**< Output drivers */
    size_t num_outputs;
    
    struct lq_sync_group **sync_groups;    /**< Sync groups */
    size_t num_sync_groups;
    
    struct lq_cyclic_ctx **cyclic_outputs; /**< Cyclic schedulers */
    size_t num_cyclic_outputs;
    
    struct lq_signal *signals;             /**< Engine signals array */
    size_t num_signals;
    
    struct lq_output_event *out_events;    /**< Output event buffer */
    size_t out_event_count;                /**< Number of pending events */
    size_t max_out_events;                 /**< Output buffer capacity */
};

/**
 * @brief Initialize engine
 * 
 * @param engine Engine instance
 * @return 0 on success, negative errno on failure
 */
int lq_engine_init(struct lq_engine *engine);

/**
 * @brief Execute one engine step
 * 
 * This is the main processing function - PURE with no RTOS calls.
 * 
 * Process flow:
 * 1. Read pending hardware samples
 * 2. Process through mid-level drivers to generate events
 * 3. Route events to output drivers
 * 4. Update synchronized groups if due
 * 
 * @param engine Engine instance
 * @param now Current timestamp (microseconds)
 * @return Number of events processed, negative errno on error
 */
int lq_engine_step(struct lq_engine *engine, uint64_t now);

#ifdef __cplusplus
}
#endif

#endif /* LQ_SYNC_GROUP_H_ */
