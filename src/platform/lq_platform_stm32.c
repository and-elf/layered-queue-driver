/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * STM32 Platform Implementation
 * 
 * Hardware: STM32F4 family
 * BLDC Motor Control: TIM1 (advanced timer with complementary outputs)
 */

#include "lq_platform.h"
#include "stm32f4xx_hal.h"

/* Timer handle for PWM generation */
static TIM_HandleTypeDef htim1;

/**
 * @brief Initialize TIM1 for 3-phase complementary PWM
 * 
 * @param pwm_freq_hz PWM frequency in Hz (typically 20kHz-40kHz)
 * @param deadtime_ns Deadtime in nanoseconds (typically 500-2000ns)
 * @return 0 on success, -1 on error
 */
static int tim1_pwm_init(uint32_t pwm_freq_hz, uint16_t deadtime_ns)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

    /* Enable TIM1 clock */
    __HAL_RCC_TIM1_CLK_ENABLE();
    
    /* Calculate timer period for desired PWM frequency */
    uint32_t timer_clock = HAL_RCC_GetPCLK2Freq() * 2;  /* APB2 timer clock */
    uint32_t prescaler = 0;  /* No prescaler for high resolution */
    uint32_t period = (timer_clock / pwm_freq_hz) - 1;
    
    /* Configure TIM1 base */
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = prescaler;
    htim1.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;  /* Center-aligned for symmetric PWM */
    htim1.Init.Period = period;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
        return -1;
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
        return -1;
    }

    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
        return -1;
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) {
        return -1;
    }

    /* Configure PWM channels (all 3 phases) */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;  /* Initially 0% duty cycle */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    
    /* Channel 1 (Phase U) */
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        return -1;
    }
    
    /* Channel 2 (Phase V) */
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
        return -1;
    }
    
    /* Channel 3 (Phase W) */
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
        return -1;
    }

    /* Configure deadtime and break (safety) */
    /* Deadtime = (DTG / Timer_Clock) in seconds */
    uint32_t dtg_value = (deadtime_ns * (timer_clock / 1000000)) / 1000;
    if (dtg_value > 255) {
        dtg_value = 255;  /* Max deadtime register value */
    }
    
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_ENABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = (uint8_t)dtg_value;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_ENABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_LOW;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK) {
        return -1;
    }

    /* Enable GPIO clocks for TIM1 pins */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    /* Configure GPIO pins for TIM1 channels */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* PA8 = TIM1_CH1 (U), PA7 = TIM1_CH1N (U_N) */
    /* PA9 = TIM1_CH2 (V), PB0 = TIM1_CH2N (V_N) */
    /* PA10 = TIM1_CH3 (W), PB1 = TIM1_CH3N (W_N) */
    
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    return 0;
}

/* =============================================================================
 * BLDC Motor Control Platform Functions
 * ========================================================================== */

int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config)
{
    (void)motor_id;  /* Only one motor supported in this implementation */
    
    if (config->num_phases != 3) {
        return -95;  /* ENOTSUP - only 3-phase supported */
    }
    
    uint16_t deadtime = config->enable_deadtime ? config->deadtime_ns : 0;
    
    /* Initialize timer with configured PWM frequency */
    int ret = tim1_pwm_init(config->pwm_frequency_hz, deadtime);
    if (ret != 0) {
        return ret;
    }
    
    /* Configure GPIO pins from config */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Map high-side pins */
    for (uint8_t phase = 0; phase < config->num_phases; phase++) {
        const struct lq_bldc_pin *hs_pin = &config->high_side_pins[phase];
        const struct lq_bldc_pin *ls_pin = &config->low_side_pins[phase];
        
        /* Enable GPIO port clocks */
        switch (hs_pin->gpio_port) {
            case 0: __HAL_RCC_GPIOA_CLK_ENABLE(); break;
            case 1: __HAL_RCC_GPIOB_CLK_ENABLE(); break;
            case 2: __HAL_RCC_GPIOC_CLK_ENABLE(); break;
            case 3: __HAL_RCC_GPIOD_CLK_ENABLE(); break;
            case 4: __HAL_RCC_GPIOE_CLK_ENABLE(); break;
        }
        
        /* Configure high-side pin */
        GPIO_TypeDef *hs_port = (GPIO_TypeDef *)(GPIOA_BASE + (hs_pin->gpio_port * 0x400));
        GPIO_InitStruct.Pin = (1U << hs_pin->gpio_pin);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = hs_pin->alternate_function;
        HAL_GPIO_Init(hs_port, &GPIO_InitStruct);
        
        /* Configure low-side (complementary) pin */
        GPIO_TypeDef *ls_port = (GPIO_TypeDef *)(GPIOA_BASE + (ls_pin->gpio_port * 0x400));
        GPIO_InitStruct.Pin = (1U << ls_pin->gpio_pin);
        GPIO_InitStruct.Alternate = ls_pin->alternate_function;
        HAL_GPIO_Init(ls_port, &GPIO_InitStruct);
    }
    
    return 0;
}

int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase, uint16_t duty_cycle)
{
    (void)motor_id;
    
    if (phase >= 3) {
        return -22;  /* EINVAL */
    }
    
    /* Calculate CCR value from duty_cycle (0-10000 = 0-100.00%) */
    uint32_t ccr_value = ((uint32_t)duty_cycle * (htim1.Init.Period + 1)) / 10000;
    
    /* Update compare register for the phase */
    switch (phase) {
        case 0:  /* Phase U */
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr_value);
            break;
        case 1:  /* Phase V */
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ccr_value);
            break;
        case 2:  /* Phase W */
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr_value);
            break;
    }
    
    return 0;
}

int lq_bldc_platform_enable(uint8_t motor_id, bool enable)
{
    (void)motor_id;
    
    if (enable) {
        /* Start PWM on all 3 channels (with complementary outputs) */
        HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
        HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
        
        HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
        HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
        
        HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
        HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
        
        /* Enable main output (MOE bit in BDTR register) */
        __HAL_TIM_MOE_ENABLE(&htim1);
    } else {
        /* Disable main output */
        __HAL_TIM_MOE_DISABLE(&htim1);
        
        /* Stop all PWM channels */
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
        HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
        
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
        HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);
        
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
        HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_3);
    }
    
    return 0;
}

int lq_bldc_platform_brake(uint8_t motor_id)
{
    (void)motor_id;
    
    /* Emergency brake: disable main output and short all low-side FETs */
    __HAL_TIM_MOE_DISABLE(&htim1);
    
    /* Set all phases to 0% duty (low-side active) */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0);
    
    return 0;
}
