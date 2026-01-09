/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Arduino Library Wrapper for BLDC Motor Driver
 */

#ifndef LAYEREDQUEUE_BLDC_H_
#define LAYEREDQUEUE_BLDC_H_

#include "lq_bldc.h"
#include "lq_platform.h"

#ifdef ARDUINO
#include <Arduino.h>

/**
 * @brief Arduino-friendly BLDC Motor class
 */
class BLDC_Motor {
public:
    /**
     * @brief Constructor
     * @param motor_id Motor instance ID (0-255)
     */
    BLDC_Motor(uint8_t motor_id = 0);
    
    /**
     * @brief Initialize motor with configuration
     * @param num_phases Number of motor phases (typically 3)
     * @param pole_pairs Number of pole pairs
     * @param pwm_freq_hz PWM frequency in Hz
     * @param deadtime_ns Deadtime in nanoseconds
     * @return true on success, false on failure
     */
    bool begin(uint8_t num_phases = 3, uint8_t pole_pairs = 7, 
               uint32_t pwm_freq_hz = 25000, uint16_t deadtime_ns = 1000);
    
    /**
     * @brief Configure high-side pin
     * @param phase Phase number (0-2 for 3-phase)
     * @param gpio_port GPIO port (0=PORTA, 1=PORTB)
     * @param gpio_pin GPIO pin number
     * @param alt_func Alternate function/peripheral mux
     */
    void setHighSidePin(uint8_t phase, uint8_t gpio_port, uint8_t gpio_pin, uint8_t alt_func);
    
    /**
     * @brief Configure low-side pin
     * @param phase Phase number (0-2 for 3-phase)
     * @param gpio_port GPIO port (0=PORTA, 1=PORTB)
     * @param gpio_pin GPIO pin number
     * @param alt_func Alternate function/peripheral mux
     */
    void setLowSidePin(uint8_t phase, uint8_t gpio_port, uint8_t gpio_pin, uint8_t alt_func);
    
    /**
     * @brief Set commutation mode
     * @param mode LQ_BLDC_MODE_6STEP, LQ_BLDC_MODE_SINE, etc.
     */
    void setMode(enum lq_bldc_mode mode);
    
    /**
     * @brief Enable or disable motor
     * @param enable true to enable, false to disable
     */
    void enable(bool enable = true);
    
    /**
     * @brief Set motor power/speed
     * @param power Power level 0-100
     */
    void setPower(uint8_t power);
    
    /**
     * @brief Set motor direction
     * @param forward true for forward, false for reverse
     */
    void setDirection(bool forward);
    
    /**
     * @brief Emergency stop with active braking
     */
    void emergencyStop();
    
    /**
     * @brief Update motor commutation (call in loop)
     * 
     * Should be called at regular intervals (e.g., 1ms)
     * for smooth motor operation.
     */
    void update();
    
    /**
     * @brief Get current motor power
     * @return Power level 0-100
     */
    uint8_t getPower() const;
    
    /**
     * @brief Check if motor is enabled
     * @return true if enabled
     */
    bool isEnabled() const;
    
private:
    struct lq_bldc_motor motor_;
    struct lq_bldc_config config_;
    uint32_t last_update_us_;
};

#endif /* ARDUINO */

#endif /* LAYEREDQUEUE_BLDC_H_ */
