/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * N-Phase BLDC Motor Driver Implementation
 */

#include "lq_bldc.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Sine wave lookup table for smoother commutation (0-360° in 256 steps) */
static const uint16_t sine_table[256] = {
    5000, 5123, 5245, 5368, 5490, 5612, 5734, 5856, 5978, 6099, 6220, 6341, 6461, 6581, 6701, 6820,
    6939, 7057, 7175, 7293, 7410, 7526, 7642, 7757, 7872, 7986, 8100, 8213, 8325, 8437, 8548, 8659,
    8769, 8878, 8987, 9095, 9202, 9309, 9415, 9520, 9624, 9728, 9831, 9933, 10034, 10135, 10235, 10334,
    10432, 10529, 10626, 10721, 10816, 10910, 11003, 11095, 11187, 11277, 11367, 11455, 11543, 11630, 11716, 11801,
    11885, 11968, 12050, 12132, 12212, 12291, 12370, 12447, 12524, 12599, 12674, 12747, 12820, 12891, 12962, 13031,
    13100, 13167, 13234, 13299, 13364, 13427, 13490, 13551, 13611, 13671, 13729, 13786, 13842, 13897, 13951, 14004,
    14056, 14107, 14156, 14205, 14252, 14299, 14344, 14388, 14431, 14473, 14514, 14554, 14593, 14631, 14668, 14703,
    14738, 14771, 14803, 14834, 14864, 14893, 14921, 14947, 14973, 14997, 15021, 15043, 15064, 15084, 15103, 15121,
    15137, 15153, 15167, 15181, 15193, 15204, 15214, 15223, 15231, 15237, 15243, 15247, 15251, 15253, 15254, 15254,
    15254, 15252, 15249, 15245, 15240, 15234, 15227, 15218, 15209, 15198, 15187, 15174, 15160, 15145, 15129, 15112,
    15094, 15075, 15055, 15033, 15011, 14987, 14963, 14937, 14911, 14883, 14854, 14824, 14794, 14762, 14729, 14695,
    14660, 14624, 14587, 14549, 14510, 14470, 14429, 14387, 14344, 14300, 14255, 14209, 14162, 14114, 14065, 14015,
    13965, 13913, 13861, 13807, 13753, 13698, 13642, 13585, 13527, 13468, 13409, 13348, 13287, 13224, 13161, 13097,
    13032, 12967, 12900, 12833, 12765, 12696, 12626, 12556, 12485, 12413, 12340, 12267, 12193, 12118, 12042, 11966,
    11889, 11811, 11733, 11654, 11574, 11494, 11413, 11331, 11249, 11166, 11082, 10998, 10913, 10827, 10741, 10655,
    10567, 10480, 10391, 10302, 10213, 10123, 10032, 9941, 9850, 9758, 9665, 9572, 9479, 9385, 9291, 9196,
};

/**
 * @brief Get sine value from lookup table
 * @param angle Electrical angle (0-65535 = 0-360°)
 * @return Sine value normalized to 0-10000 (0-100.00%)
 */
static uint16_t get_sine(uint16_t angle)
{
    uint8_t index = (uint8_t)(angle >> 8);  /* Use upper 8 bits as index */
    return sine_table[index];  /* Returns 5000-15254 (-1 to +1) */
}

/**
 * @brief Calculate 6-step commutation pattern
 */
static void calculate_6step(struct lq_bldc_motor *motor)
{
    uint8_t step = (uint8_t)(motor->state.commutation_step % 6);
    uint16_t duty = (uint16_t)((motor->state.power * motor->config.max_duty_cycle) / 100);
    
    /* Clear all phases */
    memset(motor->state.duty_cycle, 0, sizeof(motor->state.duty_cycle));
    
    /* Apply 6-step pattern (for 3-phase motor) */
    if (motor->config.num_phases == 3) {
        bool reverse = (motor->state.direction == LQ_BLDC_DIR_REVERSE);
        
        switch (step) {
            case 0: /* Phase A high, Phase B low */
                motor->state.duty_cycle[reverse ? 2 : 0] = duty;
                motor->state.duty_cycle[reverse ? 0 : 1] = 0;
                break;
            case 1: /* Phase A high, Phase C low */
                motor->state.duty_cycle[reverse ? 1 : 0] = duty;
                motor->state.duty_cycle[reverse ? 0 : 2] = 0;
                break;
            case 2: /* Phase B high, Phase C low */
                motor->state.duty_cycle[reverse ? 1 : 1] = duty;
                motor->state.duty_cycle[reverse ? 1 : 2] = 0;
                break;
            case 3: /* Phase B high, Phase A low */
                motor->state.duty_cycle[reverse ? 0 : 1] = duty;
                motor->state.duty_cycle[reverse ? 1 : 0] = 0;
                break;
            case 4: /* Phase C high, Phase A low */
                motor->state.duty_cycle[reverse ? 0 : 2] = duty;
                motor->state.duty_cycle[reverse ? 2 : 0] = 0;
                break;
            case 5: /* Phase C high, Phase B low */
                motor->state.duty_cycle[reverse ? 1 : 2] = duty;
                motor->state.duty_cycle[reverse ? 2 : 1] = 0;
                break;
        }
    }
}

/**
 * @brief Calculate sinusoidal commutation (SPWM)
 */
static void calculate_sine_pwm(struct lq_bldc_motor *motor)
{
    uint16_t angle = motor->state.electrical_angle;
    uint16_t power_scaled = (uint16_t)((motor->state.power * motor->config.max_duty_cycle) / 100);
    
    /* Calculate phase angles (120° apart for 3-phase) */
    uint16_t angle_step = (uint16_t)(65536U / motor->config.num_phases);
    
    for (uint8_t phase = 0; phase < motor->config.num_phases; phase++) {
        uint16_t phase_angle = (uint16_t)(angle + (phase * angle_step));
        
        /* Reverse direction by inverting angle */
        if (motor->state.direction == LQ_BLDC_DIR_REVERSE) {
            phase_angle = (uint16_t)(65536U - phase_angle);
        }
        
        /* Get sine value (5000-15254 range) and normalize to unipolar 0-10000 */
        uint16_t sine_val = get_sine(phase_angle);
        
        /* Convert bipolar sine (-1 to +1 = 5000-15254) to unipolar (0 to 1 = 0-10254) */
        uint32_t normalized;
        if (sine_val >= 5000U) {
            normalized = (uint32_t)(sine_val - 5000U) * 2U;  /* 0-20508 */
        } else {
            normalized = 0;  /* Negative half clipped to 0 */
        }
        
        /* Scale by power: (normalized / 20508) * power_scaled */
        motor->state.duty_cycle[phase] = (uint16_t)((normalized * power_scaled) / 20508U);
    }
}

/**
 * @brief Calculate Field-Oriented Control (FOC)
 * Simplified Clarke/Park transform approach
 */
static void calculate_foc(struct lq_bldc_motor *motor)
{
    /* For now, use sine PWM as baseline */
    /* Full FOC requires current sensing and PID loops - future enhancement */
    calculate_sine_pwm(motor);
}

/**
 * @brief Calculate open-loop V/f control
 */
static void calculate_open_loop(struct lq_bldc_motor *motor)
{
    /* Simple V/f: voltage proportional to frequency */
    calculate_sine_pwm(motor);
}

/* =============================================================================
 * Public API Implementation
 * ========================================================================== */

int lq_bldc_init(struct lq_bldc_motor *motor,
                 const struct lq_bldc_config *config,
                 uint8_t motor_id)
{
    if (!motor || !config) {
        return -22; /* EINVAL */
    }
    
    if (config->num_phases > LQ_BLDC_MAX_PHASES) {
        return -22; /* EINVAL */
    }
    
    /* Verify platform supports complementary PWM if deadtime is requested */
#if !LQ_PLATFORM_HAS_COMPLEMENTARY_PWM && !defined(LQ_PLATFORM_NATIVE)
    #if defined(__GNUC__) || defined(__clang__)
        _Static_assert(0, "Platform does not support complementary PWM channels required for BLDC control");
    #else
        #error "Platform does not support complementary PWM channels required for BLDC control"
    #endif
#endif
    
    /* Verify platform supports deadtime if enabled */
#if !LQ_PLATFORM_HAS_DEADTIME && !defined(LQ_PLATFORM_NATIVE)
    if (config->enable_deadtime) {
        return -95; /* ENOTSUP - platform doesn't support deadtime */
    }
#endif
    
    /* Copy configuration */
    memcpy(&motor->config, config, sizeof(*config));
    motor->motor_id = motor_id;
    
    /* Initialize state */
    memset(&motor->state, 0, sizeof(motor->state));
    motor->state.enabled = false;
    motor->state.power = 0;
    motor->state.direction = LQ_BLDC_DIR_FORWARD;
    
    /* Initialize platform-specific PWM */
    return lq_bldc_platform_init(motor_id, config);
}

int lq_bldc_enable(struct lq_bldc_motor *motor, bool enable)
{
    if (!motor) {
        return -22; /* EINVAL */
    }
    
    motor->state.enabled = enable;
    
    if (!enable) {
        /* Disable - set all phases to 0 */
        memset(motor->state.duty_cycle, 0, sizeof(motor->state.duty_cycle));
        for (uint8_t phase = 0; phase < motor->config.num_phases; phase++) {
            lq_bldc_platform_set_duty(motor->motor_id, phase, 0);
        }
    }
    
    return lq_bldc_platform_enable(motor->motor_id, enable);
}

int lq_bldc_set_power(struct lq_bldc_motor *motor, uint8_t power)
{
    if (!motor) {
        return -22; /* EINVAL */
    }
    
    if (power > 100) {
        power = 100;
    }
    
    motor->state.power = power;
    return 0;
}

int lq_bldc_set_direction(struct lq_bldc_motor *motor, enum lq_bldc_direction direction)
{
    if (!motor) {
        return -22; /* EINVAL */
    }
    
    motor->state.direction = direction;
    return 0;
}

int lq_bldc_update(struct lq_bldc_motor *motor, uint32_t delta_time_us)
{
    if (!motor) {
        return -22; /* EINVAL */
    }
    
    if (!motor->state.enabled || motor->state.power == 0) {
        return 0; /* Motor disabled or stopped */
    }
    
    /* Update electrical angle based on power (open-loop speed control) */
    /* In real system, this would come from back-EMF sensing or encoder */
    uint32_t rpm_target = (motor->state.power * 3000U) / 100U; /* 0-3000 RPM range */
    uint32_t angle_increment = (uint32_t)((rpm_target * 65536UL * delta_time_us) / (60UL * 1000000UL));
    motor->state.electrical_angle += (uint16_t)angle_increment;
    motor->state.mechanical_rpm = (uint16_t)rpm_target;
    
    /* Calculate commutation based on mode */
    switch (motor->config.mode) {
        case LQ_BLDC_MODE_6STEP:
            /* Update commutation step based on angle */
            motor->state.commutation_step = (uint32_t)((motor->state.electrical_angle * 6U) / 65536U);
            calculate_6step(motor);
            break;
            
        case LQ_BLDC_MODE_SINE:
            calculate_sine_pwm(motor);
            break;
            
        case LQ_BLDC_MODE_FOC:
            calculate_foc(motor);
            break;
            
        case LQ_BLDC_MODE_OPEN_LOOP:
            calculate_open_loop(motor);
            break;
            
        default:
            return -95; /* ENOTSUP */
    }
    
    /* Apply duty cycles to platform PWM */
    for (uint8_t phase = 0; phase < motor->config.num_phases; phase++) {
        lq_bldc_platform_set_duty(motor->motor_id, phase, motor->state.duty_cycle[phase]);
    }
    
    return 0;
}

int lq_bldc_emergency_stop(struct lq_bldc_motor *motor)
{
    if (!motor) {
        return -22; /* EINVAL */
    }
    
    motor->state.enabled = false;
    motor->state.power = 0;
    
    return lq_bldc_platform_brake(motor->motor_id);
}
