/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * PWM implementation using Zephyr drivers
 */

#include "lq_platform.h"

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/devicetree.h>

/* PWM channel configuration */
struct pwm_channel_config {
    const struct device *dev;
    uint32_t channel;
    pwm_flags_t flags;
};

#define MAX_PWM_CHANNELS 16
static struct pwm_channel_config pwm_channels[MAX_PWM_CHANNELS];
static bool pwm_initialized = false;

static int init_pwm_subsystem(void)
{
    if (pwm_initialized) {
        return 0;
    }
    
    for (int i = 0; i < MAX_PWM_CHANNELS; i++) {
        pwm_channels[i].dev = NULL;
        pwm_channels[i].channel = 0;
        pwm_channels[i].flags = 0;
    }
    
    pwm_initialized = true;
    return 0;
}

/* Register a PWM channel (called from generated code or app init) */
int lq_pwm_register(uint8_t channel_id, const struct device *dev, uint32_t channel, pwm_flags_t flags)
{
    init_pwm_subsystem();
    
    if (channel_id >= MAX_PWM_CHANNELS) {
        return -EINVAL;
    }
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    pwm_channels[channel_id].dev = dev;
    pwm_channels[channel_id].channel = channel;
    pwm_channels[channel_id].flags = flags;
    
    return 0;
}

int lq_pwm_set(uint8_t channel, uint16_t duty_cycle, uint32_t frequency_hz)
{
    init_pwm_subsystem();
    
    if (channel >= MAX_PWM_CHANNELS || !pwm_channels[channel].dev) {
        return -ENODEV;
    }
    
    /* Convert duty_cycle (0-10000 = 0-100.00%) to pulse width */
    uint32_t period_ns = (frequency_hz > 0) ? (1000000000UL / frequency_hz) : 20000000; /* Default 50Hz */
    uint32_t pulse_ns = (period_ns * duty_cycle) / 10000;
    
    return pwm_set(pwm_channels[channel].dev,
                   pwm_channels[channel].channel,
                   period_ns,
                   pulse_ns,
                   pwm_channels[channel].flags);
}

int lq_pwm_get(uint8_t channel, uint16_t *duty_cycle, uint32_t *frequency_hz)
{
    init_pwm_subsystem();
    
    if (channel >= MAX_PWM_CHANNELS || !pwm_channels[channel].dev) {
        return -ENODEV;
    }
    
    /* Note: Zephyr PWM API doesn't provide a get function, 
     * so we'd need to cache the values if this is needed */
    return -ENOTSUP;
}

#endif /* __ZEPHYR__ */
