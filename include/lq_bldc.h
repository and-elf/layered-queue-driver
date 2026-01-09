/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * N-Phase BLDC Motor Driver
 * 
 * Sensorless brushless DC motor control with synchronized PWM outputs.
 * Supports 3-phase (typical), 4-phase, or custom N-phase configurations.
 */

#ifndef LQ_BLDC_H_
#define LQ_BLDC_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of motor phases supported
 */
#define LQ_BLDC_MAX_PHASES 6

/**
 * @brief Commutation modes for BLDC control
 */
enum lq_bldc_mode {
    LQ_BLDC_MODE_6STEP,         /**< 6-step trapezoidal commutation (simple, efficient) */
    LQ_BLDC_MODE_SINE,          /**< Sinusoidal PWM (smoother, quieter) */
    LQ_BLDC_MODE_FOC,           /**< Field-Oriented Control (advanced, most efficient) */
    LQ_BLDC_MODE_OPEN_LOOP,     /**< Open-loop V/f control (startup, testing) */
};

/**
 * @brief Motor direction
 */
enum lq_bldc_direction {
    LQ_BLDC_DIR_FORWARD = 0,
    LQ_BLDC_DIR_REVERSE = 1,
};

/**
 * @brief BLDC motor pin configuration
 */
struct lq_bldc_pin {
    uint8_t gpio_port;           /**< GPIO port (e.g., 0=GPIOA, 1=GPIOB) */
    uint8_t gpio_pin;            /**< GPIO pin number */
    uint8_t alternate_function;  /**< Alternate function/mux setting */
};

/**
 * @brief BLDC motor configuration
 */
struct lq_bldc_config {
    uint8_t num_phases;              /**< Number of motor phases (typically 3) */
    uint8_t pole_pairs;              /**< Number of pole pairs (affects electrical angle) */
    uint32_t pwm_frequency_hz;       /**< PWM switching frequency (kHz range typical) */
    enum lq_bldc_mode mode;          /**< Commutation mode */
    uint16_t max_duty_cycle;         /**< Maximum duty cycle (0-10000 = 0-100.00%) */
    bool enable_deadtime;            /**< Enable deadtime insertion for half-bridge */
    uint16_t deadtime_ns;            /**< Deadtime in nanoseconds (typ. 500-2000ns) */
    
    /* Pin configuration (platform-specific) */
    struct lq_bldc_pin high_side_pins[LQ_BLDC_MAX_PHASES];  /**< High-side PWM pins */
    struct lq_bldc_pin low_side_pins[LQ_BLDC_MAX_PHASES];   /**< Low-side PWM pins (complementary) */
};

/**
 * @brief BLDC motor state
 */
struct lq_bldc_state {
    bool enabled;                    /**< Motor enable/disable */
    uint8_t power;                   /**< Power/speed setpoint (0-100) */
    enum lq_bldc_direction direction; /**< Rotation direction */
    uint16_t electrical_angle;       /**< Current electrical angle (0-65535 = 0-360°) */
    uint16_t mechanical_rpm;         /**< Estimated mechanical RPM (sensorless estimation) */
    uint32_t commutation_step;       /**< Current commutation step (6-step mode) */
    uint16_t duty_cycle[LQ_BLDC_MAX_PHASES]; /**< Per-phase duty cycles (0-10000) */
};

/**
 * @brief BLDC motor instance
 */
struct lq_bldc_motor {
    struct lq_bldc_config config;
    struct lq_bldc_state state;
    uint8_t motor_id;                /**< Motor instance ID (for multi-motor systems) */
};

/**
 * @brief Initialize BLDC motor driver
 * 
 * @param motor Motor instance to initialize
 * @param config Motor configuration
 * @param motor_id Unique motor ID (0-255)
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_init(struct lq_bldc_motor *motor, 
                 const struct lq_bldc_config *config,
                 uint8_t motor_id);

/**
 * @brief Enable/disable motor
 * 
 * @param motor Motor instance
 * @param enable true to enable, false to disable (coast)
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_enable(struct lq_bldc_motor *motor, bool enable);

/**
 * @brief Set motor power/speed
 * 
 * @param motor Motor instance
 * @param power Power level (0-100, where 0=stop, 100=full power)
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_set_power(struct lq_bldc_motor *motor, uint8_t power);

/**
 * @brief Set motor direction
 * 
 * @param motor Motor instance
 * @param direction Forward or reverse
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_set_direction(struct lq_bldc_motor *motor, enum lq_bldc_direction direction);

/**
 * @brief Update commutation (call periodically or from timer ISR)
 * 
 * This function calculates the next electrical angle and updates
 * PWM duty cycles for all phases based on the commutation mode.
 * 
 * @param motor Motor instance
 * @param delta_time_us Time since last update in microseconds
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_update(struct lq_bldc_motor *motor, uint32_t delta_time_us);

/**
 * @brief Emergency stop (active brake)
 * 
 * @param motor Motor instance
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_emergency_stop(struct lq_bldc_motor *motor);

/* =============================================================================
 * Platform-specific functions (implement in lq_platform_*.c)
 * ========================================================================== */

/**
 * @brief Platform-specific PWM initialization
 * 
 * Configure PWM timers, channels, and GPIO pins for motor control.
 * 
 * @param motor_id Motor instance ID
 * @param config Complete motor configuration including pins
 * @return 0 on success, negative errno on failure
 */
extern int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config);

/**
 * @brief Set PWM duty cycle for a specific phase
 * 
 * @param motor_id Motor instance ID
 * @param phase Phase number (0 to num_phases-1)
 * @param duty_cycle Duty cycle (0-10000 = 0-100.00%)
 * @return 0 on success, negative errno on failure
 */
extern int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase, uint16_t duty_cycle);

/**
 * @brief Enable/disable all PWM outputs for a motor
 * 
 * @param motor_id Motor instance ID
 * @param enable true to enable PWM outputs, false to disable (high-Z)
 * @return 0 on success, negative errno on failure
 */
extern int lq_bldc_platform_enable(uint8_t motor_id, bool enable);

/**
 * @brief Emergency stop - set all phases low (active brake)
 * 
 * @param motor_id Motor instance ID
 * @return 0 on success, negative errno on failure
 */
extern int lq_bldc_platform_brake(uint8_t motor_id);

#ifdef __cplusplus
}
#endif

#endif /* LQ_BLDC_H_ */
