/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * STM32 BLDC Motor Control Application Example
 * 
 * This example demonstrates how to use the BLDC motor driver API
 * in an application. The platform-specific implementation is in
 * src/platform/lq_platform_stm32.c
 */

#include "lq_bldc.h"
#include "lq_engine.h"
#include "lq_platform.h"

/* Motor instance */
static struct lq_bldc_motor motor;

/* =============================================================================
 * Application Example
 * ========================================================================== */

void bldc_example_init(void)
{
    /* Configure motor parameters */
    struct lq_bldc_config config = {
        .num_phases = 3,
        .pole_pairs = 7,  /* Common for drone/RC motors */
        .mode = LQ_BLDC_MODE_SINE,  /* Smooth sinusoidal commutation */
        .pwm_frequency_hz = 25000,  /* 25kHz PWM */
        .max_duty_cycle = 9500,  /* 95% max duty (safety margin) */
        .enable_deadtime = true,
        .deadtime_ns = 1000,  /* 1μs deadtime */
        
        /* STM32 TIM1 pin configuration */
        .high_side_pins = {
            {.gpio_port = 0, .gpio_pin = 8,  .alternate_function = 1},  /* PA8  = TIM1_CH1  */
            {.gpio_port = 0, .gpio_pin = 9,  .alternate_function = 1},  /* PA9  = TIM1_CH2  */
            {.gpio_port = 0, .gpio_pin = 10, .alternate_function = 1},  /* PA10 = TIM1_CH3  */
        },
        .low_side_pins = {
            {.gpio_port = 0, .gpio_pin = 7, .alternate_function = 1},   /* PA7 = TIM1_CH1N */
            {.gpio_port = 1, .gpio_pin = 0, .alternate_function = 1},   /* PB0 = TIM1_CH2N */
            {.gpio_port = 1, .gpio_pin = 1, .alternate_function = 1},   /* PB1 = TIM1_CH3N */
        },
    };
    
    /* Initialize motor */
    lq_bldc_init(&motor, &config, 0);
}

void bldc_example_control(uint8_t throttle_input)
{
    /* Throttle input is 0-100 from RC receiver or joystick */
    static uint32_t last_update = 0;
    uint32_t now = lq_platform_uptime_get();
    
    /* Update at 1kHz (1ms period) */
    if (now - last_update >= 1) {
        uint32_t delta_us = (now - last_update) * 1000;
        
        /* Set motor power from throttle */
        lq_bldc_set_power(&motor, throttle_input);
        
        /* Enable motor if throttle > 0 */
        if (throttle_input > 0 && !motor.state.enabled) {
            lq_bldc_enable(&motor, true);
        } else if (throttle_input == 0 && motor.state.enabled) {
            lq_bldc_enable(&motor, false);
        }
        
        /* Update commutation */
        lq_bldc_update(&motor, delta_us);
        
        last_update = now;
    }
}

void emergency_stop_handler(void)
{
    /* Called on external interrupt (e.g., kill switch) */
    lq_bldc_emergency_stop(&motor);
}
