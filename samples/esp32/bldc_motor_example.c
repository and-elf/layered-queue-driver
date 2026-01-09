/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP32 BLDC Motor Control Application Example
 * 
 * This example demonstrates how to use the BLDC motor driver API
 * on ESP32. The platform-specific implementation is in
 * src/platform/lq_platform_esp32.c
 */

#include "lq_bldc.h"
#include "lq_engine.h"
#include "lq_platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
        
        /* ESP32 MCPWM pin configuration */
        .high_side_pins = {
            {.gpio_port = 0, .gpio_pin = 16, .alternate_function = 0},  /* GPIO16 = MCPWM0A */
            {.gpio_port = 0, .gpio_pin = 18, .alternate_function = 0},  /* GPIO18 = MCPWM1A */
            {.gpio_port = 0, .gpio_pin = 19, .alternate_function = 0},  /* GPIO19 = MCPWM2A */
        },
        .low_side_pins = {
            {.gpio_port = 0, .gpio_pin = 17, .alternate_function = 0},  /* GPIO17 = MCPWM0B */
            {.gpio_port = 0, .gpio_pin = 5,  .alternate_function = 0},  /* GPIO5  = MCPWM1B */
            {.gpio_port = 0, .gpio_pin = 4,  .alternate_function = 0},  /* GPIO4  = MCPWM2B */
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

/* =============================================================================
 * FreeRTOS Task Example
 * ========================================================================== */

void motor_control_task(void *pvParameters)
{
    uint8_t throttle = 0;
    
    bldc_example_init();
    
    while (1) {
        /* Get throttle input from ADC, RC receiver, etc. */
        /* throttle = read_throttle_input(); */
        
        /* For demo, ramp up and down */
        static bool ramp_up = true;
        if (ramp_up) {
            throttle++;
            if (throttle >= 50) ramp_up = false;
        } else {
            throttle--;
            if (throttle == 0) ramp_up = true;
        }
        
        bldc_example_control(throttle);
        
        vTaskDelay(pdMS_TO_TICKS(1));  /* 1ms update rate */
    }
}

void app_main(void)
{
    /* Create motor control task */
    xTaskCreate(motor_control_task, "motor_ctrl", 4096, NULL, 5, NULL);
}
