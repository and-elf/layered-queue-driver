/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Verified Output Driver
 * 
 * Commands hardware outputs and verifies they actually occurred by
 * reading back verification signals. Sets FAULT status on mismatch.
 */

#ifndef LQ_VERIFIED_OUTPUT_H
#define LQ_VERIFIED_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct lq_engine;

/**
 * @brief Verified output hardware type
 */
enum lq_verified_output_type {
    LQ_VERIFIED_GPIO = 0,    /**< Digital GPIO (0/1) */
    LQ_VERIFIED_PWM,         /**< PWM duty cycle */
    LQ_VERIFIED_ANALOG,      /**< Analog voltage/current */
    LQ_VERIFIED_POSITION,    /**< Linear/rotary position */
    LQ_VERIFIED_SPEED,       /**< Motor speed */
};

/**
 * @brief Verified output context
 * 
 * Monitors commanded output vs actual measured output and sets fault
 * status if they don't match within tolerance.
 */
struct lq_verified_output_ctx {
    uint8_t command_signal;      /**< Desired output command */
    uint8_t verification_signal; /**< Actual measured output */
    uint8_t output_signal;       /**< Verified output (with status) */
    
    enum lq_verified_output_type output_type; /**< Type of output */
    
    int32_t tolerance;           /**< Max allowed error */
    uint64_t verify_timeout_us;  /**< Time to wait before verifying */
    bool continuous_verify;      /**< Continuous vs one-shot verify */
    
    /* Runtime state */
    uint64_t command_timestamp;  /**< When command was last changed */
    int32_t last_command;        /**< Last commanded value */
    bool waiting_for_verify;     /**< Waiting for timeout to expire */
    
    bool enabled;                /**< Enable/disable this output */
};

/**
 * @brief Process verified outputs
 * 
 * Checks that commanded outputs match verification signals.
 * Sets output signal status to FAULT on mismatch.
 * 
 * @param engine Engine context
 * @param outputs Array of verified output contexts
 * @param num_outputs Number of outputs to process
 * @param now Current timestamp in microseconds
 */
void lq_process_verified_outputs(
    struct lq_engine *engine,
    struct lq_verified_output_ctx *outputs,
    uint8_t num_outputs,
    uint64_t now);

#endif /* LQ_VERIFIED_OUTPUT_H */
