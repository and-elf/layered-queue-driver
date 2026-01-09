/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * SAMD BLDC Motor Example
 * 
 * Hardware: SAMD21/SAMD51 with TCC0 for 3-phase motor control
 * Application: Simple BLDC motor control with throttle input
 */

#include "lq_bldc.h"
#include "lq_platform.h"
#include <stdio.h>

/* Motor instance */
static struct lq_bldc_motor motor;

/**
 * @brief Initialize BLDC motor example
 */
int bldc_example_init(void)
{
    struct lq_bldc_config config = {
        .num_phases = 3,
        .pole_pairs = 7,
        .pwm_frequency_hz = 25000,  /* 25 kHz PWM */
        .mode = LQ_BLDC_MODE_SINE,
        .max_duty_cycle = 9500,     /* 95% max duty for safety margin */
        .enable_deadtime = true,
        .deadtime_ns = 1000,        /* 1 microsecond deadtime */
        
        /* TCC0 Pin Configuration for SAMD21/SAMD51 */
        /* High-side outputs on WO[0], WO[1], WO[2] */
        /* Low-side outputs on WO[4], WO[5], WO[6] (complementary) */
        .high_side_pins = {
            /* Phase U: PA04 = TCC0/WO[0], peripheral function E */
            {.gpio_port = 0, .gpio_pin = 4, .alternate_function = 0x04},  /* PORT_PMUX_E */
            
            /* Phase V: PA05 = TCC0/WO[1], peripheral function E */
            {.gpio_port = 0, .gpio_pin = 5, .alternate_function = 0x04},
            
            /* Phase W: PA06 = TCC0/WO[2], peripheral function E */
            {.gpio_port = 0, .gpio_pin = 6, .alternate_function = 0x04},
        },
        .low_side_pins = {
            /* Phase U_N: PA10 = TCC0/WO[4], peripheral function E */
            {.gpio_port = 0, .gpio_pin = 10, .alternate_function = 0x04},
            
            /* Phase V_N: PA11 = TCC0/WO[5], peripheral function E */
            {.gpio_port = 0, .gpio_pin = 11, .alternate_function = 0x04},
            
            /* Phase W_N: PA12 = TCC0/WO[6], peripheral function E */
            {.gpio_port = 0, .gpio_pin = 12, .alternate_function = 0x04},
        },
    };
    
    /* Initialize motor driver */
    int ret = lq_bldc_init(&motor, &config, 0);
    if (ret != 0) {
        printf("BLDC init failed: %d\n", ret);
        return ret;
    }
    
    printf("BLDC motor initialized on TCC0\n");
    printf("PWM frequency: %lu Hz\n", config.pwm_frequency_hz);
    printf("Deadtime: %u ns\n", config.deadtime_ns);
    
    return 0;
}

/**
 * @brief BLDC motor control loop
 * 
 * @param throttle Throttle input 0-100
 */
void bldc_example_control(uint8_t throttle)
{
    /* Set motor power */
    lq_bldc_set_power(&motor, throttle);
    
    /* Update commutation (call at regular intervals, e.g., 1kHz) */
    lq_bldc_commutate(&motor);
}

/**
 * @brief Main function for Arduino-style SAMD boards
 */
void setup(void)
{
    /* Initialize serial for debugging */
    /* Serial.begin(115200); // Arduino style */
    
    /* Initialize BLDC motor */
    if (bldc_example_init() != 0) {
        while (1); /* Halt on error */
    }
    
    /* Enable motor */
    lq_bldc_enable(&motor, true);
}

void loop(void)
{
    /* Simple ramp test */
    static uint8_t throttle = 0;
    static bool increasing = true;
    
    /* Update motor control */
    bldc_example_control(throttle);
    
    /* Ramp throttle up and down */
    if (increasing) {
        throttle++;
        if (throttle >= 100) {
            increasing = false;
        }
    } else {
        throttle--;
        if (throttle == 0) {
            increasing = true;
        }
    }
    
    /* 1ms update rate */
    lq_platform_delay_ms(1);
}

/**
 * @brief Main function for bare-metal SAMD
 */
int main(void)
{
    setup();
    
    while (1) {
        loop();
    }
    
    return 0;
}
