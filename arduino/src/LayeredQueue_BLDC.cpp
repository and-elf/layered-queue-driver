/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Arduino Library Wrapper Implementation
 */

#ifdef ARDUINO

#include "LayeredQueue_BLDC.h"

BLDC_Motor::BLDC_Motor(uint8_t motor_id)
    : last_update_us_(0)
{
    memset(&motor_, 0, sizeof(motor_));
    memset(&config_, 0, sizeof(config_));
    
    motor_.motor_id = motor_id;
    
    /* Default configuration */
    config_.num_phases = 3;
    config_.pole_pairs = 7;
    config_.pwm_frequency_hz = 25000;
    config_.mode = LQ_BLDC_MODE_SINE;
    config_.max_duty_cycle = 9500;
    config_.enable_deadtime = true;
    config_.deadtime_ns = 1000;
}

bool BLDC_Motor::begin(uint8_t num_phases, uint8_t pole_pairs,
                       uint32_t pwm_freq_hz, uint16_t deadtime_ns)
{
    config_.num_phases = num_phases;
    config_.pole_pairs = pole_pairs;
    config_.pwm_frequency_hz = pwm_freq_hz;
    config_.deadtime_ns = deadtime_ns;
    
    int ret = lq_bldc_init(&motor_, &config_, motor_.motor_id);
    
    if (ret == 0) {
        last_update_us_ = micros();
    }
    
    return (ret == 0);
}

void BLDC_Motor::setHighSidePin(uint8_t phase, uint8_t gpio_port,
                                 uint8_t gpio_pin, uint8_t alt_func)
{
    if (phase < LQ_BLDC_MAX_PHASES) {
        config_.high_side_pins[phase].gpio_port = gpio_port;
        config_.high_side_pins[phase].gpio_pin = gpio_pin;
        config_.high_side_pins[phase].alternate_function = alt_func;
    }
}

void BLDC_Motor::setLowSidePin(uint8_t phase, uint8_t gpio_port,
                                uint8_t gpio_pin, uint8_t alt_func)
{
    if (phase < LQ_BLDC_MAX_PHASES) {
        config_.low_side_pins[phase].gpio_port = gpio_port;
        config_.low_side_pins[phase].gpio_pin = gpio_pin;
        config_.low_side_pins[phase].alternate_function = alt_func;
    }
}

void BLDC_Motor::setMode(enum lq_bldc_mode mode)
{
    config_.mode = mode;
    motor_.config.mode = mode;
}

void BLDC_Motor::enable(bool enable)
{
    lq_bldc_enable(&motor_, enable);
}

void BLDC_Motor::setPower(uint8_t power)
{
    lq_bldc_set_power(&motor_, power);
}

void BLDC_Motor::setDirection(bool forward)
{
    lq_bldc_set_direction(&motor_, forward ? LQ_BLDC_DIR_FORWARD : LQ_BLDC_DIR_REVERSE);
}

void BLDC_Motor::emergencyStop()
{
    lq_bldc_emergency_stop(&motor_);
}

void BLDC_Motor::update()
{
    uint32_t now_us = micros();
    uint32_t delta_us = now_us - last_update_us_;
    
    lq_bldc_update(&motor_, delta_us);
    
    last_update_us_ = now_us;
}

uint8_t BLDC_Motor::getPower() const
{
    return motor_.state.power;
}

bool BLDC_Motor::isEnabled() const
{
    return motor_.state.enabled;
}

#endif /* ARDUINO */
