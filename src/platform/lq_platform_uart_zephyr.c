/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * UART implementation using Zephyr drivers
 */

#include "lq_platform.h"

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>

/* UART device bindings from devicetree */
#define UART0_NODE DT_NODELABEL(uart0)
#define UART1_NODE DT_NODELABEL(uart1)
#define UART2_NODE DT_NODELABEL(uart2)

static const struct device *uart_devices[] = {
#if DT_NODE_EXISTS(UART0_NODE)
    DEVICE_DT_GET(UART0_NODE),
#else
    NULL,
#endif
#if DT_NODE_EXISTS(UART1_NODE)
    DEVICE_DT_GET(UART1_NODE),
#else
    NULL,
#endif
#if DT_NODE_EXISTS(UART2_NODE)
    DEVICE_DT_GET(UART2_NODE),
#else
    NULL,
#endif
};

#define NUM_UARTS ARRAY_SIZE(uart_devices)

int lq_uart_send(uint8_t port, const uint8_t *data, uint16_t length)
{
    if (port >= NUM_UARTS || !uart_devices[port]) {
        return -ENODEV;
    }
    
    const struct device *dev = uart_devices[port];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    /* Send data byte by byte */
    for (uint16_t i = 0; i < length; i++) {
        uart_poll_out(dev, data[i]);
    }
    
    return 0;
}

int lq_uart_recv(uint8_t port, uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    if (port >= NUM_UARTS || !uart_devices[port]) {
        return -ENODEV;
    }
    
    const struct device *dev = uart_devices[port];
    
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    
    uint64_t deadline = k_uptime_get() + timeout_ms;
    uint16_t received = 0;
    
    while (received < length) {
        unsigned char c;
        int ret = uart_poll_in(dev, &c);
        
        if (ret == 0) {
            data[received++] = c;
        } else if (k_uptime_get() >= deadline) {
            return -ETIMEDOUT;
        } else {
            k_sleep(K_USEC(100));  /* Small delay to avoid busy-wait */
        }
    }
    
    return received;
}

#endif /* __ZEPHYR__ */
