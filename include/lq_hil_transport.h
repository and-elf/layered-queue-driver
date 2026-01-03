/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * HIL Transport Abstraction Layer
 * 
 * Allows HIL testing over different transports:
 * - Unix sockets (Linux native)
 * - UART (embedded → PC)
 * - USB CDC (embedded → PC)
 * - TCP/IP (ESP32/networked devices → PC)
 */

#ifndef LQ_HIL_TRANSPORT_H
#define LQ_HIL_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Transport types */
enum lq_hil_transport_type {
    LQ_HIL_TRANSPORT_UNIX,      /* Unix domain sockets (native Linux) */
    LQ_HIL_TRANSPORT_UART,      /* UART serial port */
    LQ_HIL_TRANSPORT_USB_CDC,   /* USB CDC (virtual COM port) */
    LQ_HIL_TRANSPORT_TCP,       /* TCP/IP socket */
};

/* Transport configuration */
struct lq_hil_transport_config {
    enum lq_hil_transport_type type;
    
    union {
        /* Unix sockets (current implementation) */
        struct {
            int pid;  /* Process ID for socket naming */
        } unix_socket;
        
        /* UART */
        struct {
            const char *device;  /* e.g., "/dev/ttyUSB0" */
            uint32_t baudrate;   /* e.g., 115200 */
        } uart;
        
        /* USB CDC */
        struct {
            const char *device;  /* e.g., "/dev/ttyACM0" */
        } usb_cdc;
        
        /* TCP */
        struct {
            const char *host;    /* IP address or hostname */
            uint16_t port;       /* TCP port */
        } tcp;
    };
};

/* Transport interface (function pointers) */
struct lq_hil_transport {
    /* Initialize transport */
    int (*init)(const struct lq_hil_transport_config *config, bool is_server);
    
    /* Send data */
    int (*send)(const void *data, size_t len);
    
    /* Receive data (blocking with timeout) */
    int (*recv)(void *data, size_t len, int timeout_ms);
    
    /* Cleanup */
    void (*cleanup)(void);
};

/**
 * Get transport implementation for given type
 */
const struct lq_hil_transport *lq_hil_get_transport(enum lq_hil_transport_type type);

/**
 * Initialize HIL with specific transport
 * 
 * @param config Transport configuration
 * @param is_server True for SUT, false for tester
 * @return 0 on success
 */
int lq_hil_transport_init(const struct lq_hil_transport_config *config, bool is_server);

#ifdef __cplusplus
}
#endif

#endif /* LQ_HIL_TRANSPORT_H */
