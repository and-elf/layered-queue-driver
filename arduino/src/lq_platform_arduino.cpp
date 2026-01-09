/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Arduino Platform Abstraction Layer
 * 
 * Provides basic time and platform functions for Arduino
 */

#ifdef ARDUINO

#include "lq_platform.h"
#include <Arduino.h>

/* Time functions */
uint32_t lq_platform_uptime_get(void)
{
    return millis();
}

uint64_t lq_platform_get_time_us(void)
{
    return micros();
}

void lq_platform_sleep_ms(uint32_t ms)
{
    delay(ms);
}

void lq_platform_sleep_us(uint32_t us)
{
    delayMicroseconds(us);
}

/* Stub implementations for unused functions in Arduino context */
int lq_gpio_set(uint8_t pin, bool state)
{
    digitalWrite(pin, state ? HIGH : LOW);
    return 0;
}

int lq_pwm_set(uint8_t channel, uint32_t duty_cycle)
{
    /* Not used - motor driver handles PWM directly */
    return 0;
}

int lq_dac_write(uint8_t channel, uint16_t value)
{
    /* Not typically available on Arduino boards */
    (void)channel;
    (void)value;
    return -1;
}

int lq_modbus_write(uint8_t slave_id, uint16_t reg, uint16_t value)
{
    /* Not used in basic motor control */
    (void)slave_id;
    (void)reg;
    (void)value;
    return -1;
}

#endif /* ARDUINO */
