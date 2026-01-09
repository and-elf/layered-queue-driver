/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO implementation using Zephyr drivers
 */

#include "lq_platform.h"

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>

/* GPIO pin mapping structure */
struct gpio_pin_config {
    const struct device *port;
    gpio_pin_t pin;
    gpio_flags_t flags;
};

/* Maximum number of GPIO pins we can track */
#define MAX_GPIO_PINS 64
static struct gpio_pin_config gpio_pins[MAX_GPIO_PINS];
static bool gpio_initialized = false;

/* Initialize GPIO subsystem - should be called at startup */
static int init_gpio_subsystem(void)
{
    if (gpio_initialized) {
        return 0;
    }
    
    /* Initialize all pins as unassigned */
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        gpio_pins[i].port = NULL;
        gpio_pins[i].pin = 0;
        gpio_pins[i].flags = 0;
    }
    
    gpio_initialized = true;
    return 0;
}

/* Register a GPIO pin (called from generated code or app init) */
int lq_gpio_register(uint8_t pin_id, const struct device *port, gpio_pin_t pin, gpio_flags_t flags)
{
    init_gpio_subsystem();
    
    if (pin_id >= MAX_GPIO_PINS) {
        return -EINVAL;
    }
    
    if (!device_is_ready(port)) {
        return -ENODEV;
    }
    
    gpio_pins[pin_id].port = port;
    gpio_pins[pin_id].pin = pin;
    gpio_pins[pin_id].flags = flags;
    
    /* Configure the pin */
    return gpio_pin_configure(port, pin, flags);
}

int lq_gpio_set(uint8_t pin, bool value)
{
    init_gpio_subsystem();
    
    if (pin >= MAX_GPIO_PINS || !gpio_pins[pin].port) {
        return -ENODEV;
    }
    
    return gpio_pin_set(gpio_pins[pin].port, gpio_pins[pin].pin, value ? 1 : 0);
}

int lq_gpio_get(uint8_t pin, bool *value)
{
    init_gpio_subsystem();
    
    if (pin >= MAX_GPIO_PINS || !gpio_pins[pin].port) {
        return -ENODEV;
    }
    
    if (!value) {
        return -EINVAL;
    }
    
    int ret = gpio_pin_get(gpio_pins[pin].port, gpio_pins[pin].pin);
    if (ret < 0) {
        return ret;
    }
    
    *value = (ret != 0);
    return 0;
}

int lq_gpio_toggle(uint8_t pin)
{
    init_gpio_subsystem();
    
    if (pin >= MAX_GPIO_PINS || !gpio_pins[pin].port) {
        return -ENODEV;
    }
    
    return gpio_pin_toggle(gpio_pins[pin].port, gpio_pins[pin].pin);
}

#endif /* __ZEPHYR__ */
