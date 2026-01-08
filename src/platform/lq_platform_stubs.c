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
int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len) {
    (void)can_id;
    (void)is_extended;
    (void)data;
    (void)len;
    return 0;  /* Stub - no actual transmission */
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
int lq_uart_send(const uint8_t *data, size_t len) {
    (void)data;
    (void)len;
    return 0;  /* Stub - no actual UART transmission */
}

/* =============================================================================
 * SPI Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_spi_send(uint8_t device, const uint8_t *data, size_t len) {
    (void)device;
    (void)data;
    (void)len;
    return 0;  /* Stub - no actual SPI transmission */
}

/* =============================================================================
 * I2C Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len) {
    (void)addr;
    (void)reg;
    (void)data;
    (void)len;
    return 0;  /* Stub - no actual I2C write */
}

/* =============================================================================
 * PWM Functions
 * ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_pwm_set(uint8_t channel, uint32_t duty_cycle) {
    (void)channel;
    (void)duty_cycle;
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
