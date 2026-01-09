/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPI implementation using Zephyr drivers
 */

#include "lq_platform.h"

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>

/* SPI device configuration */
struct spi_device_config {
    const struct device *bus;
    struct spi_config config;
    struct spi_cs_control cs_ctrl;
};

#define MAX_SPI_DEVICES 8
static struct spi_device_config spi_devices[MAX_SPI_DEVICES];
static bool spi_initialized = false;

static int init_spi_subsystem(void)
{
    if (spi_initialized) {
        return 0;
    }
    
    for (int i = 0; i < MAX_SPI_DEVICES; i++) {
        spi_devices[i].bus = NULL;
    }
    
    spi_initialized = true;
    return 0;
}

/* Register an SPI device (called from generated code or app init) */
int lq_spi_register(uint8_t device_id, 
                    const struct device *bus,
                    const struct gpio_dt_spec *cs_gpio,
                    uint32_t frequency,
                    uint16_t operation)
{
    init_spi_subsystem();
    
    if (device_id >= MAX_SPI_DEVICES) {
        return -EINVAL;
    }
    
    if (!device_is_ready(bus)) {
        return -ENODEV;
    }
    
    /* Setup CS control if provided */
    if (cs_gpio && cs_gpio->port) {
        if (!device_is_ready(cs_gpio->port)) {
            return -ENODEV;
        }
        
        spi_devices[device_id].cs_ctrl.gpio = *cs_gpio;
        spi_devices[device_id].cs_ctrl.delay = 0;
    }
    
    /* Configure SPI parameters */
    spi_devices[device_id].bus = bus;
    spi_devices[device_id].config.frequency = frequency;
    spi_devices[device_id].config.operation = operation;
    spi_devices[device_id].config.slave = 0;
    spi_devices[device_id].config.cs = cs_gpio ? &spi_devices[device_id].cs_ctrl : NULL;
    
    return 0;
}

int lq_spi_send(uint8_t cs_pin, const uint8_t *data, uint16_t length)
{
    init_spi_subsystem();
    
    if (cs_pin >= MAX_SPI_DEVICES || !spi_devices[cs_pin].bus) {
        return -ENODEV;
    }
    
    const struct spi_buf tx_buf = {
        .buf = (void *)data,
        .len = length
    };
    
    const struct spi_buf_set tx = {
        .buffers = &tx_buf,
        .count = 1
    };
    
    return spi_write(spi_devices[cs_pin].bus, 
                     &spi_devices[cs_pin].config,
                     &tx);
}

int lq_spi_recv(uint8_t cs_pin, uint8_t *data, uint16_t length)
{
    init_spi_subsystem();
    
    if (cs_pin >= MAX_SPI_DEVICES || !spi_devices[cs_pin].bus) {
        return -ENODEV;
    }
    
    const struct spi_buf rx_buf = {
        .buf = data,
        .len = length
    };
    
    const struct spi_buf_set rx = {
        .buffers = &rx_buf,
        .count = 1
    };
    
    return spi_read(spi_devices[cs_pin].bus,
                    &spi_devices[cs_pin].config,
                    &rx);
}

int lq_spi_transceive(uint8_t cs_pin, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    init_spi_subsystem();
    
    if (cs_pin >= MAX_SPI_DEVICES || !spi_devices[cs_pin].bus) {
        return -ENODEV;
    }
    
    const struct spi_buf tx_buf = {
        .buf = (void *)tx_data,
        .len = length
    };
    
    const struct spi_buf_set tx = {
        .buffers = &tx_buf,
        .count = 1
    };
    
    const struct spi_buf rx_buf = {
        .buf = rx_data,
        .len = length
    };
    
    const struct spi_buf_set rx = {
        .buffers = &rx_buf,
        .count = 1
    };
    
    return spi_transceive(spi_devices[cs_pin].bus,
                         &spi_devices[cs_pin].config,
                         &tx, &rx);
}

#endif /* __ZEPHYR__ */
