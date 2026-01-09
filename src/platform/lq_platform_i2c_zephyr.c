/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * I2C implementation using Zephyr drivers
 */

#include "lq_platform.h"

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/devicetree.h>

/* I2C bus configuration */
#define I2C0_NODE DT_NODELABEL(i2c0)
#define I2C1_NODE DT_NODELABEL(i2c1)

static const struct device *i2c_buses[] = {
#if DT_NODE_EXISTS(I2C0_NODE)
    DEVICE_DT_GET(I2C0_NODE),
#else
    NULL,
#endif
#if DT_NODE_EXISTS(I2C1_NODE)
    DEVICE_DT_GET(I2C1_NODE),
#else
    NULL,
#endif
};

#define NUM_I2C_BUSES ARRAY_SIZE(i2c_buses)

/* Default I2C bus to use (can be overridden) */
static uint8_t default_i2c_bus = 0;

int lq_i2c_set_default_bus(uint8_t bus_id)
{
    if (bus_id >= NUM_I2C_BUSES || !i2c_buses[bus_id]) {
        return -ENODEV;
    }
    
    if (!device_is_ready(i2c_buses[bus_id])) {
        return -ENODEV;
    }
    
    default_i2c_bus = bus_id;
    return 0;
}

int lq_i2c_write(uint8_t address, const uint8_t *data, uint16_t length)
{
    if (default_i2c_bus >= NUM_I2C_BUSES || !i2c_buses[default_i2c_bus]) {
        return -ENODEV;
    }
    
    const struct device *dev = i2c_buses[default_i2c_bus];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    return i2c_write(dev, data, length, address);
}

int lq_i2c_read(uint8_t address, uint8_t *data, uint16_t length)
{
    if (default_i2c_bus >= NUM_I2C_BUSES || !i2c_buses[default_i2c_bus]) {
        return -ENODEV;
    }
    
    const struct device *dev = i2c_buses[default_i2c_bus];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    return i2c_read(dev, data, length, address);
}

int lq_i2c_write_read(uint8_t address, 
                      const uint8_t *write_data, uint16_t write_length,
                      uint8_t *read_data, uint16_t read_length)
{
    if (default_i2c_bus >= NUM_I2C_BUSES || !i2c_buses[default_i2c_bus]) {
        return -ENODEV;
    }
    
    const struct device *dev = i2c_buses[default_i2c_bus];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    return i2c_write_read(dev, address, 
                         write_data, write_length,
                         read_data, read_length);
}

int lq_i2c_reg_write_byte(uint8_t address, uint8_t reg, uint8_t value)
{
    if (default_i2c_bus >= NUM_I2C_BUSES || !i2c_buses[default_i2c_bus]) {
        return -ENODEV;
    }
    
    const struct device *dev = i2c_buses[default_i2c_bus];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    return i2c_reg_write_byte(dev, address, reg, value);
}

int lq_i2c_reg_read_byte(uint8_t address, uint8_t reg, uint8_t *value)
{
    if (default_i2c_bus >= NUM_I2C_BUSES || !i2c_buses[default_i2c_bus]) {
        return -ENODEV;
    }
    
    const struct device *dev = i2c_buses[default_i2c_bus];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    return i2c_reg_read_byte(dev, address, reg, value);
}

int lq_i2c_burst_write(uint8_t address, uint8_t start_reg, const uint8_t *data, uint16_t length)
{
    if (default_i2c_bus >= NUM_I2C_BUSES || !i2c_buses[default_i2c_bus]) {
        return -ENODEV;
    }
    
    const struct device *dev = i2c_buses[default_i2c_bus];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    return i2c_burst_write(dev, address, start_reg, data, length);
}

int lq_i2c_burst_read(uint8_t address, uint8_t start_reg, uint8_t *data, uint16_t length)
{
    if (default_i2c_bus >= NUM_I2C_BUSES || !i2c_buses[default_i2c_bus]) {
        return -ENODEV;
    }
    
    const struct device *dev = i2c_buses[default_i2c_bus];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    return i2c_burst_read(dev, address, start_reg, data, length);
}

#endif /* __ZEPHYR__ */
