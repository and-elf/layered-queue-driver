/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO Pattern Driver - Generates periodic GPIO patterns (blink, PWM-like)
 */

#ifndef LQ_GPIO_PATTERN_H
#define LQ_GPIO_PATTERN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct lq_engine;

/**
 * @brief GPIO pattern types
 */
enum lq_gpio_pattern_type {
    LQ_GPIO_PATTERN_STATIC = 0,    /**< Static on/off (no pattern) */
    LQ_GPIO_PATTERN_BLINK,         /**< Simple blink (50% duty cycle) */
    LQ_GPIO_PATTERN_PWM,           /**< PWM-like with configurable duty cycle */
    LQ_GPIO_PATTERN_CUSTOM,        /**< Custom bit pattern */
};

/**
 * @brief GPIO pattern context structure
 *
 * Generates periodic patterns on GPIO pins. Can be driven by signals
 * (for FMI integration) or run independently (for simple LED blink).
 */
struct lq_gpio_pattern_ctx {
    uint8_t gpio_pin;              /**< GPIO pin ID */
    uint8_t control_signal;        /**< Optional: Signal ID to control pattern (0xFF = none) */
    enum lq_gpio_pattern_type type; /**< Pattern type */

    /* Timing parameters (in microseconds) */
    uint32_t period_us;            /**< Pattern period in microseconds */
    uint32_t on_time_us;           /**< Time GPIO is high (for BLINK/PWM) */

    /* Custom pattern support */
    uint32_t pattern_bits;         /**< Custom bit pattern (up to 32 bits) */
    uint8_t pattern_length;        /**< Number of bits in pattern (1-32) */

    /* State */
    uint64_t last_update_us;       /**< Last update timestamp */
    uint32_t phase_us;             /**< Current phase within period */
    uint8_t pattern_index;         /**< Current position in custom pattern */
    bool current_state;            /**< Current GPIO state */
    bool enabled;                  /**< Pattern generation enabled */
    bool inverted;                 /**< Invert output */
};

/**
 * @brief Process GPIO patterns
 *
 * Updates GPIO outputs based on patterns and timing. Should be called
 * periodically (e.g., from main loop or timer).
 *
 * @param engine Engine context (can be NULL if not using signal control)
 * @param patterns Array of pattern contexts
 * @param num_patterns Number of patterns to process
 * @param now Current timestamp in microseconds
 */
void lq_process_gpio_patterns(
    struct lq_engine *engine,
    struct lq_gpio_pattern_ctx *patterns,
    uint8_t num_patterns,
    uint64_t now);

/**
 * @brief Initialize a GPIO pattern context
 *
 * @param ctx Pattern context to initialize
 * @param gpio_pin GPIO pin ID
 * @param type Pattern type
 * @param period_us Period in microseconds
 */
void lq_gpio_pattern_init(
    struct lq_gpio_pattern_ctx *ctx,
    uint8_t gpio_pin,
    enum lq_gpio_pattern_type type,
    uint32_t period_us);

/**
 * @brief Set pattern duty cycle (for BLINK/PWM patterns)
 *
 * @param ctx Pattern context
 * @param duty_percent Duty cycle percentage (0-100)
 */
void lq_gpio_pattern_set_duty(
    struct lq_gpio_pattern_ctx *ctx,
    uint8_t duty_percent);

/**
 * @brief Set custom bit pattern
 *
 * @param ctx Pattern context
 * @param pattern Bit pattern (LSB first)
 * @param length Number of bits (1-32)
 */
void lq_gpio_pattern_set_custom(
    struct lq_gpio_pattern_ctx *ctx,
    uint32_t pattern,
    uint8_t length);

/**
 * @brief Enable/disable pattern generation
 *
 * @param ctx Pattern context
 * @param enabled True to enable, false to disable
 */
void lq_gpio_pattern_enable(
    struct lq_gpio_pattern_ctx *ctx,
    bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* LQ_GPIO_PATTERN_H */
