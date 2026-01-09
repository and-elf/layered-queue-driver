/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP32 Platform Implementation
 * 
 * Hardware: ESP32, ESP32-S3
 * BLDC Motor Control: MCPWM peripheral (Motor Control PWM)
 */

#include "lq_platform.h"
#include "driver/mcpwm.h"
#include "driver/gpio.h"
#include "soc/mcpwm_periph.h"

/* MCPWM unit assignment per motor (ESP32 has 2 units, ESP32-S3 has 1) */
static mcpwm_unit_t get_mcpwm_unit(uint8_t motor_id)
{
#if defined(CONFIG_IDF_TARGET_ESP32)
    /* ESP32 has MCPWM0 and MCPWM1 */
    return (motor_id == 0) ? MCPWM_UNIT_0 : MCPWM_UNIT_1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    /* ESP32-S3 has only MCPWM0 */
    (void)motor_id;
    return MCPWM_UNIT_0;
#else
    (void)motor_id;
    return MCPWM_UNIT_0;
#endif
}

/**
 * @brief Initialize MCPWM for 3-phase complementary PWM
 * 
 * @param motor_id Motor instance ID
 * @param config Complete motor configuration including pins
 * @return 0 on success, -1 on error
 */
static int mcpwm_init_3phase(uint8_t motor_id, const struct lq_bldc_config *config)
{
    mcpwm_unit_t unit = get_mcpwm_unit(motor_id);
    
    /* Configure GPIO pins from config */
    for (uint8_t phase = 0; phase < config->num_phases; phase++) {
        const struct lq_bldc_pin *hs_pin = &config->high_side_pins[phase];
        const struct lq_bldc_pin *ls_pin = &config->low_side_pins[phase];
        
        /* Calculate actual GPIO pin number from port and pin */
        int hs_gpio = (hs_pin->gpio_port * 32) + hs_pin->gpio_pin;
        int ls_gpio = (ls_pin->gpio_port * 32) + ls_pin->gpio_pin;
        
        /* Map phase to MCPWM operator and timer */
        mcpwm_timer_t timer;
        mcpwm_io_signals_t pwm_a, pwm_b;
        
        switch (phase) {
            case 0:  /* Phase U */
                timer = MCPWM_TIMER_0;
                pwm_a = MCPWM0A;
                pwm_b = MCPWM0B;
                break;
            case 1:  /* Phase V */
                timer = MCPWM_TIMER_1;
                pwm_a = MCPWM1A;
                pwm_b = MCPWM1B;
                break;
            case 2:  /* Phase W */
                timer = MCPWM_TIMER_2;
                pwm_a = MCPWM2A;
                pwm_b = MCPWM2B;
                break;
            default:
                return -22;  /* EINVAL */
        }
        
        /* Set GPIO for MCPWM outputs */
        mcpwm_gpio_init(unit, pwm_a, hs_gpio);
        mcpwm_gpio_init(unit, pwm_b, ls_gpio);
    }
    
    /* Configure MCPWM with center-aligned mode */
    mcpwm_config_t pwm_config = {
        .frequency = config->pwm_frequency_hz,
        .cmpr_a = 0,  /* Initially 0% duty */
        .cmpr_b = 0,
        .duty_mode = MCPWM_DUTY_MODE_0,
        .counter_mode = MCPWM_UP_DOWN_COUNTER,  /* Center-aligned */
    };
    
    /* Initialize all 3 timers */
    for (uint8_t phase = 0; phase < config->num_phases; phase++) {
        mcpwm_timer_t timer = (mcpwm_timer_t)phase;
        
        if (mcpwm_init(unit, timer, &pwm_config) != ESP_OK) {
            return -1;
        }
        
        /* Configure deadtime if enabled */
        if (config->enable_deadtime) {
            /* Convert nanoseconds to MCPWM clock cycles (160MHz) */
            uint32_t dt_cycles = (config->deadtime_ns * 160U) / 1000U;
            
            mcpwm_deadtime_type_t dt_type = MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE;
            
            if (mcpwm_deadtime_enable(unit, timer, dt_type, dt_cycles, dt_cycles) != ESP_OK) {
                return -1;
            }
        }
        
        /* Set initial duty to 0 */
        mcpwm_set_duty(unit, timer, MCPWM_OPR_A, 0);
        mcpwm_set_duty(unit, timer, MCPWM_OPR_B, 0);
        mcpwm_set_duty_type(unit, timer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
        mcpwm_set_duty_type(unit, timer, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
    }
    
    return 0;
}

/* =============================================================================
 * BLDC Motor Control Platform Functions
 * ========================================================================== */

int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config)
{
    if (config->num_phases != 3) {
        return -95;  /* ENOTSUP - only 3-phase supported */
    }
    
#if defined(CONFIG_IDF_TARGET_ESP32)
    if (motor_id >= 2) {
        return -22;  /* EINVAL - ESP32 has only 2 MCPWM units */
    }
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    if (motor_id >= 1) {
        return -22;  /* EINVAL - ESP32-S3 has only 1 MCPWM unit */
    }
#endif
    
    return mcpwm_init_3phase(motor_id, config);
}

int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase, uint16_t duty_cycle)
{
    if (phase >= 3) {
        return -22;  /* EINVAL */
    }
    
    mcpwm_unit_t unit = get_mcpwm_unit(motor_id);
    mcpwm_timer_t timer = (mcpwm_timer_t)phase;
    
    /* Convert duty_cycle (0-10000 = 0-100.00%) to percentage (0-100) */
    float duty_percent = (float)duty_cycle / 100.0f;
    
    /* In complementary mode, PWM_A is the duty cycle, PWM_B is complement */
    mcpwm_set_duty(unit, timer, MCPWM_OPR_A, duty_percent);
    
    return 0;
}

int lq_bldc_platform_enable(uint8_t motor_id, bool enable)
{
    mcpwm_unit_t unit = get_mcpwm_unit(motor_id);
    
    if (enable) {
        /* Start all 3 timers */
        for (uint8_t phase = 0; phase < 3; phase++) {
            mcpwm_timer_t timer = (mcpwm_timer_t)phase;
            mcpwm_start(unit, timer);
        }
    } else {
        /* Stop all 3 timers */
        for (uint8_t phase = 0; phase < 3; phase++) {
            mcpwm_timer_t timer = (mcpwm_timer_t)phase;
            mcpwm_stop(unit, timer);
        }
    }
    
    return 0;
}

int lq_bldc_platform_brake(uint8_t motor_id)
{
    mcpwm_unit_t unit = get_mcpwm_unit(motor_id);
    
    /* Set all phases to 0% duty (low-side active) and stop */
    for (uint8_t phase = 0; phase < 3; phase++) {
        mcpwm_timer_t timer = (mcpwm_timer_t)phase;
        mcpwm_set_duty(unit, timer, MCPWM_OPR_A, 0);
        mcpwm_set_duty(unit, timer, MCPWM_OPR_B, 0);
        mcpwm_stop(unit, timer);
    }
    
    return 0;
}
