/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PID Controller Driver
 * 
 * Implements closed-loop control using measured feedback.
 */

#ifndef LQ_PID_H
#define LQ_PID_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct lq_engine;

/**
 * @brief PID controller context
 * 
 * Implements proportional-integral-derivative control for maintaining
 * a setpoint using measured feedback.
 */
struct lq_pid_ctx {
    /* Signal connections */
    uint8_t setpoint_signal;     /**< Desired target value */
    uint8_t measurement_signal;  /**< Actual measured value (feedback) */
    uint8_t output_signal;       /**< PID control output */
    
    /* PID gains (scaled by 1000) */
    int32_t kp;                  /**< Proportional gain * 1000 */
    int32_t ki;                  /**< Integral gain * 1000 */
    int32_t kd;                  /**< Derivative gain * 1000 */
    
    /* Output limits */
    int32_t output_min;          /**< Minimum output value */
    int32_t output_max;          /**< Maximum output value */
    
    /* Integral anti-windup */
    int32_t integral_min;        /**< Min integral accumulator */
    int32_t integral_max;        /**< Max integral accumulator */
    
    /* Configuration */
    int32_t deadband;            /**< Error deadband for integral */
    uint64_t sample_time_us;     /**< Fixed sample time (0=variable) */
    bool reset_on_setpoint_change; /**< Reset integral on setpoint change */
    
    /* Runtime state */
    int64_t integral;            /**< Accumulated error (64-bit for range) */
    int32_t last_error;          /**< Previous error for derivative */
    int32_t last_setpoint;       /**< Previous setpoint (detect changes) */
    uint64_t last_time;          /**< Previous timestamp */
    bool first_run;              /**< First execution flag */
    
    bool enabled;                /**< Enable/disable controller */
};

/**
 * @brief Process PID controllers
 * 
 * Calculates PID control outputs based on setpoint and measurement.
 * 
 * @param engine Engine context
 * @param pids Array of PID controller contexts
 * @param num_pids Number of PIDs to process
 * @param now Current timestamp in microseconds
 */
void lq_process_pids(
    struct lq_engine *engine,
    struct lq_pid_ctx *pids,
    uint8_t num_pids,
    uint64_t now);

#endif /* LQ_PID_H */
