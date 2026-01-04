/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Engine core
 * 
 * The engine is the pure processing heart of the system.
 * It ingests events, maintains canonical signals, processes merges,
 * and generates output events - all without RTOS dependencies.
 */

#ifndef LQ_ENGINE_H_
#define LQ_ENGINE_H_

#include "lq_event.h"
#include "lq_mid_driver.h"
#include "lq_common.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration (from KConfig or defaults)
 * ============================================================================ */

#ifndef LQ_MAX_SIGNALS
#define LQ_MAX_SIGNALS 32           /**< Maximum number of signals */
#endif

#ifndef LQ_MAX_CYCLIC_OUTPUTS
#define LQ_MAX_CYCLIC_OUTPUTS 16    /**< Maximum cyclic output contexts */
#endif

#ifndef LQ_MAX_OUTPUT_EVENTS
#define LQ_MAX_OUTPUT_EVENTS 64     /**< Output event buffer size */
#endif

#ifndef LQ_MAX_MERGES
#define LQ_MAX_MERGES 8             /**< Maximum merge/voter contexts */
#endif

#ifndef LQ_MAX_FAULT_MONITORS
#define LQ_MAX_FAULT_MONITORS 8     /**< Maximum fault monitor contexts */
#endif

/* ============================================================================
 * Core structures
 * ============================================================================ */

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
    uint64_t stale_us;            /**< Staleness timeout (0 = no check) */
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
    bool enabled;                 /**< Enable/disable output */
};

/**
 * @brief Merge context
 * 
 * Combines multiple input signals using voting algorithms.
 */
struct lq_merge_ctx {
    uint8_t input_signals[8];     /**< Input signal indices (max 8 inputs) */
    uint8_t num_inputs;           /**< Number of inputs */
    uint8_t output_signal;        /**< Output signal index */
    
    enum lq_vote_method voting_method;  /**< Voting algorithm */
    
    uint32_t tolerance;           /**< Maximum allowed deviation */
    uint64_t stale_us;            /**< Staleness timeout */
    bool enabled;                 /**< Enable/disable merge */
};

/**
 * @brief Fault monitor context
 * 
 * Monitors signals for fault conditions and sets a fault output signal.
 * Supports multiple fault detection strategies:
 * - Staleness: Input signal hasn't updated within timeout
 * - Range: Input value outside min/max bounds
 * - Merge failure: Voting/merge produced FAULT status
 */
struct lq_fault_monitor_ctx {
    uint8_t input_signal;         /**< Signal to monitor */
    uint8_t fault_output_signal;  /**< Fault flag output (0=OK, 1=FAULT) */
    
    /* Fault conditions */
    bool check_staleness;         /**< Enable staleness check */
    uint64_t stale_timeout_us;    /**< Staleness timeout */
    
    bool check_range;             /**< Enable range check */
    int32_t min_value;            /**< Minimum valid value */
    int32_t max_value;            /**< Maximum valid value */
    
    bool check_status;            /**< Check for LQ_EVENT_FAULT status */
    
    bool enabled;                 /**< Enable/disable monitor */
};

/**
 * @brief Engine configuration
 * 
 * Top-level structure with fixed-size arrays for deterministic memory usage.
 * Array sizes configured via KConfig.
 */
struct lq_engine {
    /* Signal storage - canonical values */
    struct lq_signal signals[LQ_MAX_SIGNALS];
    uint8_t num_signals;
    
    /* Merge/voter contexts */
    struct lq_merge_ctx merges[LQ_MAX_MERGES];
    uint8_t num_merges;
    
    /* Fault monitor contexts */
    struct lq_fault_monitor_ctx fault_monitors[LQ_MAX_FAULT_MONITORS];
    uint8_t num_fault_monitors;
    
    /* Cyclic output schedulers */
    struct lq_cyclic_ctx cyclic_outputs[LQ_MAX_CYCLIC_OUTPUTS];
    uint8_t num_cyclic_outputs;
    
    /* Output event buffer */
    struct lq_output_event out_events[LQ_MAX_OUTPUT_EVENTS];
    uint8_t out_event_count;
};

/* ============================================================================
 * Engine API
 * ============================================================================ */

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
 * 1. Ingest events from mid-level drivers into signals
 * 2. Apply staleness detection to all inputs
 * 3. Process merge/voter logic
 * 4. Process on-change outputs
 * 5. Process cyclic outputs (deadline-based scheduling)
 * 
 * @param engine Engine instance
 * @param now Current timestamp (microseconds)
 * @param events Array of events from mid-level drivers
 * @param n_events Number of events to process
 */
void lq_engine_step(
    struct lq_engine *engine,
    uint64_t now,
    const struct lq_event *events,
    size_t n_events);

/* ============================================================================
 * Engine step internal phases
 * ============================================================================ */

/**
 * @brief Ingest events into engine signals
 * 
 * Updates the canonical signal values from incoming events.
 * Marks signals as updated for downstream processing.
 * 
 * @param e Engine instance
 * @param events Input events
 * @param n_events Number of events
 */
void lq_ingest_events(
    struct lq_engine *e,
    const struct lq_event *events,
    size_t n_events);

/**
 * @brief Apply staleness detection to all inputs
 * 
 * Checks signal timestamps against configured staleness timeouts.
 * Updates signal status to LQ_EVENT_TIMEOUT if stale.
 * 
 * @param e Engine instance
 * @param now Current timestamp
 */
void lq_apply_input_staleness(
    struct lq_engine *e,
    uint64_t now);

/**
 * @brief Process merge/voter logic
 * 
 * Runs voting algorithms on redundant inputs.
 * Updates merged signal values.
 * 
 * @param e Engine instance
 * @param now Current timestamp
 */
void lq_process_merges(
    struct lq_engine *e,
    uint64_t now);

/**
 * @brief Process fault monitors
 * 
 * Checks fault conditions on monitored signals.
 * Sets fault output signals based on detection results.
 * 
 * @param e Engine instance
 * @param now Current timestamp
 */
void lq_process_fault_monitors(
    struct lq_engine *e,
    uint64_t now);

/**
 * @brief Process on-change outputs
 * 
 * Generates output events for signals that have changed
 * and meet their trigger conditions.
 * 
 * @param e Engine instance
 */
void lq_process_outputs(struct lq_engine *e);

/**
 * @brief Process cyclic outputs
 * 
 * Checks all cyclic contexts and generates output events
 * for those that have reached their deadline.
 * 
 * @param e Engine instance
 * @param now Current timestamp
 */
void lq_process_cyclic_outputs(
    struct lq_engine *e,
    uint64_t now);

/* ============================================================================
 * Engine task API
 * ============================================================================ */

/**
 * @brief Get the global engine instance
 * 
 * @return Pointer to global engine instance
 */
struct lq_engine *lq_engine_get_instance(void);

/**
 * @brief Start engine processing task (for manual control)
 * 
 * Only needed if CONFIG_LQ_ENGINE_TASK is not enabled.
 * Creates platform-specific thread/task to run engine.
 * 
 * @return 0 on success, negative errno on failure
 */
int lq_engine_task_start(void);

/**
 * @brief Run engine task loop (for bare metal)
 * 
 * Never returns. Call from main() in bare metal systems.
 * Only available if CONFIG_LQ_ENGINE_TASK_BARE_METAL is set.
 */
void lq_engine_task_run(void);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Process single cyclic output context
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
    if (!c->enabled || now < c->next_deadline)
        return;
    
    if (e->out_event_count >= LQ_MAX_OUTPUT_EVENTS)
        return;  /* Buffer full */
    
    const struct lq_signal *sig = &e->signals[c->source_signal];
    struct lq_output_event *evt = &e->out_events[e->out_event_count++];
    
    evt->type = c->type;
    evt->target_id = c->target_id;
    evt->value = sig->value;
    evt->flags = c->flags;
    evt->timestamp = now;
    
    c->next_deadline += c->period_us;
}

#ifdef __cplusplus
}
#endif

#endif /* LQ_ENGINE_H_ */
