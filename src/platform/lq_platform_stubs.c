/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform function stubs
 * 
 * This file provides default no-op implementations of platform functions.
 * 
 * Usage:
 * 1. GNU toolchains: Link this file to get weak default implementations
 *    that can be overridden by your platform-specific code.
 * 
 * 2. Non-GNU toolchains: Either:
 *    a) Don't link this file and provide your own implementations, OR
 *    b) Link this file and use linker options to allow multiple definitions
 * 
 * 3. Testing: Link this file to build tests without real hardware.
 */

#include "lq_platform.h"

/* =============================================================================
 * CAN Bus Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_can_send(uint8_t device_index, uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len) {
    (void)device_index;
    (void)can_id;
    (void)is_extended;
    (void)data;
    (void)len;
    return 0;  /* Stub - no actual transmission */
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_can_recv(uint8_t device_index, uint32_t *can_id, bool *is_extended, uint8_t *data, uint8_t *len, uint32_t timeout_ms) {
    (void)device_index;
    (void)can_id;
    (void)is_extended;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return -1;  /* Stub - no actual reception */
}

/* =============================================================================
 * GPIO Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_gpio_set(uint8_t pin, bool state) {
    (void)pin;
    (void)state;
    return 0;  /* Stub - no actual GPIO control */
}

/* =============================================================================
 * UART Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_uart_send(uint8_t port, const uint8_t *data, uint16_t length) {
    (void)port;
    (void)data;
    (void)length;
    return 0;  /* Stub - no actual UART transmission */
}

/* =============================================================================
 * SPI Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_spi_send(uint8_t cs_pin, const uint8_t *data, uint16_t length) {
    (void)cs_pin;
    (void)data;
    (void)length;
    return 0;  /* Stub - no actual SPI transmission */
}

/* =============================================================================
 * I2C Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_i2c_write(uint8_t address, const uint8_t *data, uint16_t length) {
    (void)address;
    (void)data;
    (void)length;
    return 0;  /* Stub - no actual I2C write */
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_i2c_read(uint8_t address, uint8_t *data, uint16_t length) {
    (void)address;
    (void)data;
    (void)length;
    return 0;  /* Stub - no actual I2C read */
}

/* =============================================================================
 * PWM Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_pwm_set(uint8_t channel, uint16_t duty_cycle, uint32_t frequency_hz) {
    (void)channel;
    (void)duty_cycle;
    (void)frequency_hz;
    return 0;  /* Stub - no actual PWM control */
}

/* =============================================================================
 * DAC Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_dac_write(uint8_t channel, uint16_t value) {
    (void)channel;
    (void)value;
    return 0;  /* Stub - no actual DAC write */
}

/* =============================================================================
 * Modbus Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_modbus_write(uint8_t slave_id, uint16_t reg, uint16_t value) {
    (void)slave_id;
    (void)reg;
    (void)value;
    return 0;  /* Stub - no actual Modbus write */
}

/* =============================================================================
 * BLDC Motor Control Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config) {
    (void)motor_id;
    (void)config;
    return 0;  /* Stub - no actual PWM initialization */
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase, uint16_t duty_cycle) {
    (void)motor_id;
    (void)phase;
    (void)duty_cycle;
    return 0;  /* Stub - no actual duty cycle update */
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_bldc_platform_enable(uint8_t motor_id, bool enable) {
    (void)motor_id;
    (void)enable;
    return 0;  /* Stub - no actual enable/disable */
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_bldc_platform_brake(uint8_t motor_id) {
    (void)motor_id;
    return 0;  /* Stub - no actual braking */
}
