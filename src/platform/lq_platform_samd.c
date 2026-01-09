/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * SAMD Platform Implementation
 * 
 * Hardware: SAMD21/SAMD51 series
 * BLDC Motor Control: TCC0 (Timer/Counter for Control with complementary outputs)
 */

#include "lq_platform.h"
#include "lq_bldc.h"
#include <errno.h>

/* SAMD HAL headers - adapt based on ASF4 or Arduino Core */
#ifdef SAMD_ASF4
    #include "hri_tcc.h"
    #include "hri_port.h"
    #include "hri_gclk.h"
    #include "hri_mclk.h"
#else
    /* Arduino SAMD Core or custom HAL */
    #include "sam.h"
#endif

/* TCC module instance for motor 0 (TCC0 has 4 channels, good for 3-phase) */
/* TCC1 and TCC2 can be used for additional motors on SAMD51 */
static Tcc *tcc_motors[] = {
    TCC0,  /* Motor 0 on TCC0 */
#if defined(__SAMD51__)
    TCC1,  /* Motor 1 on TCC1 (SAMD51 only) */
#endif
};

#define NUM_TCC_MOTORS (sizeof(tcc_motors) / sizeof(tcc_motors[0]))

/**
 * @brief Get TCC module for motor ID
 */
static Tcc* get_tcc_module(uint8_t motor_id)
{
    if (motor_id >= NUM_TCC_MOTORS) {
        return NULL;
    }
    return tcc_motors[motor_id];
}

/**
 * @brief Enable clock for TCC module
 */
static void tcc_enable_clock(uint8_t motor_id)
{
#if defined(__SAMD51__)
    /* SAMD51: Use MCLK and GCLK */
    switch (motor_id) {
        case 0:
            MCLK->APBBMASK.bit.TCC0_ = 1;
            GCLK->PCHCTRL[TCC0_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
        case 1:
            MCLK->APBBMASK.bit.TCC1_ = 1;
            GCLK->PCHCTRL[TCC1_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
    }
#elif defined(__SAMD21__)
    /* SAMD21: Use PM and GCLK */
    PM->APBCMASK.bit.TCC0_ = 1;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_TCC0_TCC1 | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
#endif
}

/**
 * @brief Configure GPIO pin for TCC output
 * 
 * @param port GPIO port (0=PORTA, 1=PORTB)
 * @param pin GPIO pin number
 * @param pmux_func Peripheral multiplexer function (e.g., PORT_PMUX_PMUXE_E for TCC0)
 */
static void configure_tcc_pin(uint8_t port, uint8_t pin, uint8_t pmux_func)
{
    Port *port_base = (port == 0) ? PORT : (PORT + 1);
    
    /* Set pin as peripheral function */
    port_base->Group[0].PINCFG[pin].bit.PMUXEN = 1;
    
    /* Configure peripheral mux (even/odd pins use different fields) */
    if (pin & 1) {
        port_base->Group[0].PMUX[pin >> 1].bit.PMUXO = pmux_func;
    } else {
        port_base->Group[0].PMUX[pin >> 1].bit.PMUXE = pmux_func;
    }
}

/**
 * @brief Initialize TCC for 3-phase complementary PWM
 * 
 * @param tcc TCC module instance
 * @param pwm_freq_hz PWM frequency in Hz (typically 20kHz-40kHz)
 * @param deadtime_ns Deadtime in nanoseconds (typically 500-2000ns)
 * @return 0 on success, -1 on error
 */
static int tcc_3phase_pwm_init(Tcc *tcc, uint32_t pwm_freq_hz, uint16_t deadtime_ns)
{
    if (tcc == NULL) {
        return -EINVAL;
    }
    
    /* Disable TCC before configuration */
    tcc->CTRLA.bit.ENABLE = 0;
    while (tcc->SYNCBUSY.bit.ENABLE);
    
    /* Reset TCC */
    tcc->CTRLA.bit.SWRST = 1;
    while (tcc->SYNCBUSY.bit.SWRST);
    
    /* Calculate period for desired PWM frequency */
    /* SAMD21: 48 MHz system clock, SAMD51: 120 MHz */
#if defined(__SAMD51__)
    uint32_t tcc_clock = 120000000UL;
#else
    uint32_t tcc_clock = 48000000UL;
#endif
    
    /* Use prescaler DIV1 for highest resolution */
    uint32_t period = (tcc_clock / pwm_freq_hz) - 1;
    
    /* Configure TCC */
    tcc->CTRLA.reg = TCC_CTRLA_PRESCALER_DIV1 |   /* No prescaler */
                     TCC_CTRLA_PRESCSYNC_GCLK;     /* Prescaler synchronization */
    
    /* Set waveform generation mode: Normal PWM (NPWM) */
    tcc->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;         /* Normal PWM */
    while (tcc->SYNCBUSY.bit.WAVE);
    
    /* Set period (TOP value) */
    tcc->PER.reg = period;
    while (tcc->SYNCBUSY.bit.PER);
    
    /* Initialize all compare channels to 0% duty cycle */
    tcc->CC[0].reg = 0;
    tcc->CC[1].reg = 0;
    tcc->CC[2].reg = 0;
    while (tcc->SYNCBUSY.bit.CC0 || tcc->SYNCBUSY.bit.CC1 || tcc->SYNCBUSY.bit.CC2);
    
    /* Configure deadtime insertion */
    if (deadtime_ns > 0) {
        /* Deadtime clock = TCC clock / prescaler */
        /* Deadtime cycles = (deadtime_ns * tcc_clock) / 1000000000 */
        uint32_t dt_cycles = (deadtime_ns * (tcc_clock / 1000000)) / 1000;
        
        /* SAMD TCC deadtime is 8-bit, max 255 cycles */
        if (dt_cycles > 255) {
            dt_cycles = 255;
        }
        
        /* Enable deadtime for all outputs */
        tcc->WEXCTRL.reg = TCC_WEXCTRL_OTMX(0) |           /* Default output matrix */
                          TCC_WEXCTRL_DTIEN0 |             /* Deadtime on channel 0 */
                          TCC_WEXCTRL_DTIEN1 |             /* Deadtime on channel 1 */
                          TCC_WEXCTRL_DTIEN2 |             /* Deadtime on channel 2 */
                          TCC_WEXCTRL_DTLS((uint8_t)dt_cycles) |  /* Low-side deadtime */
                          TCC_WEXCTRL_DTHS((uint8_t)dt_cycles);   /* High-side deadtime */
        while (tcc->SYNCBUSY.bit.WEXCTRL);
    }
    
    /* Enable pattern generation for complementary outputs */
    /* Pattern buffer controls output states during off time */
    tcc->PATT.reg = 0;  /* Default: all outputs follow CC registers */
    while (tcc->SYNCBUSY.bit.PATT);
    
    /* Enable TCC */
    tcc->CTRLA.bit.ENABLE = 1;
    while (tcc->SYNCBUSY.bit.ENABLE);
    
    return 0;
}

/* =============================================================================
 * Platform API Implementation
 * ========================================================================== */

/**
 * @brief Initialize BLDC motor PWM hardware
 */
int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config)
{
    if (config == NULL) {
        return -EINVAL;
    }
    
    if (config->num_phases > 3) {
        /* TCC0 has 4 channels, but we reserve one for advanced features */
        return -ENOTSUP;
    }
    
    Tcc *tcc = get_tcc_module(motor_id);
    if (tcc == NULL) {
        return -EINVAL;
    }
    
    /* Enable clock for this TCC module */
    tcc_enable_clock(motor_id);
    
    /* Configure GPIO pins based on config */
    for (uint8_t i = 0; i < config->num_phases; i++) {
        /* High-side pin */
        configure_tcc_pin(config->high_side_pins[i].gpio_port,
                         config->high_side_pins[i].gpio_pin,
                         config->high_side_pins[i].alternate_function);
        
        /* Low-side pin (complementary) */
        configure_tcc_pin(config->low_side_pins[i].gpio_port,
                         config->low_side_pins[i].gpio_pin,
                         config->low_side_pins[i].alternate_function);
    }
    
    /* Initialize TCC with PWM parameters */
    uint16_t deadtime = config->enable_deadtime ? config->deadtime_ns : 0;
    return tcc_3phase_pwm_init(tcc, config->pwm_frequency_hz, deadtime);
}

/**
 * @brief Set PWM duty cycle for a motor phase
 */
int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase, uint16_t duty_cycle)
{
    Tcc *tcc = get_tcc_module(motor_id);
    if (tcc == NULL || phase >= 3) {
        return -EINVAL;
    }
    
    /* Convert 0-10000 (0.01% units) to TCC compare value */
    uint32_t period = tcc->PER.reg;
    uint32_t compare = (duty_cycle * period) / 10000;
    
    /* Update compare register */
    tcc->CCBUF[phase].reg = compare;
    
    return 0;
}

/**
 * @brief Enable/disable motor PWM outputs
 */
int lq_bldc_platform_enable(uint8_t motor_id, bool enable)
{
    Tcc *tcc = get_tcc_module(motor_id);
    if (tcc == NULL) {
        return -EINVAL;
    }
    
    if (enable) {
        /* Enable all outputs */
        tcc->CTRLA.bit.ENABLE = 1;
        while (tcc->SYNCBUSY.bit.ENABLE);
    } else {
        /* Disable all outputs */
        tcc->CTRLA.bit.ENABLE = 0;
        while (tcc->SYNCBUSY.bit.ENABLE);
    }
    
    return 0;
}

/**
 * @brief Emergency brake motor
 */
int lq_bldc_platform_brake(uint8_t motor_id)
{
    Tcc *tcc = get_tcc_module(motor_id);
    if (tcc == NULL) {
        return -EINVAL;
    }
    
    /* Set all duty cycles to 0 */
    tcc->CCBUF[0].reg = 0;
    tcc->CCBUF[1].reg = 0;
    tcc->CCBUF[2].reg = 0;
    
    /* Force pattern to safe state (all low) */
    tcc->PATTBUF.reg = 0xFF;  /* All outputs low */
    
    /* Disable outputs */
    tcc->CTRLA.bit.ENABLE = 0;
    while (tcc->SYNCBUSY.bit.ENABLE);
    
    return 0;
}
