/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hardware-in-the-Loop (HIL) Testing Interface
 * 
 * Enables software-based HIL testing using Unix domain sockets for IPC
 * between test runner and system under test (SUT).
 */

#ifndef LQ_HIL_H
#define LQ_HIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HIL message types */
enum lq_hil_msg_type {
    LQ_HIL_MSG_ADC = 0,      /* ADC sample */
    LQ_HIL_MSG_SPI = 1,      /* SPI transaction */
    LQ_HIL_MSG_CAN = 2,      /* CAN frame */
    LQ_HIL_MSG_GPIO = 3,     /* GPIO state */
    LQ_HIL_MSG_SYNC = 4,     /* Synchronization */
};

/* HIL message header */
struct lq_hil_msg_hdr {
    uint8_t type;            /* lq_hil_msg_type */
    uint8_t channel;         /* Channel/pin number */
    uint16_t length;         /* Payload length */
    uint64_t timestamp_us;   /* Microseconds */
} __attribute__((packed));

/* ADC injection message */
struct lq_hil_adc_msg {
    struct lq_hil_msg_hdr hdr;
    uint32_t value;          /* ADC reading (raw or scaled) */
} __attribute__((packed));

/* SPI injection message */
struct lq_hil_spi_msg {
    struct lq_hil_msg_hdr hdr;
    uint8_t data[32];        /* SPI payload */
} __attribute__((packed));

/* CAN injection/capture message */
struct lq_hil_can_msg {
    struct lq_hil_msg_hdr hdr;
    uint32_t can_id;         /* 11-bit or 29-bit ID */
    uint8_t is_extended;     /* 1 = 29-bit, 0 = 11-bit */
    uint8_t dlc;             /* Data length (0-8) */
    uint8_t data[8];         /* CAN data */
} __attribute__((packed));

/* GPIO state message */
struct lq_hil_gpio_msg {
    struct lq_hil_msg_hdr hdr;
    uint8_t pin;             /* GPIO pin number */
    uint8_t state;           /* 0 = low, 1 = high */
} __attribute__((packed));

/* Sync message (for timing measurements) */
struct lq_hil_sync_msg {
    struct lq_hil_msg_hdr hdr;
    uint32_t sequence;       /* Sequence number */
} __attribute__((packed));

/* HIL socket paths (runtime with PID) */
#define LQ_HIL_SOCKET_ADC   "/tmp/lq_hil_adc_%d"
#define LQ_HIL_SOCKET_SPI   "/tmp/lq_hil_spi_%d"
#define LQ_HIL_SOCKET_CAN   "/tmp/lq_hil_can_%d"
#define LQ_HIL_SOCKET_GPIO  "/tmp/lq_hil_gpio_%d"
#define LQ_HIL_SOCKET_SYNC  "/tmp/lq_hil_sync_%d"

/* HIL mode control */
enum lq_hil_mode {
    LQ_HIL_MODE_DISABLED = 0,  /* Normal hardware mode */
    LQ_HIL_MODE_SUT = 1,       /* System Under Test (listens on sockets) */
    LQ_HIL_MODE_TESTER = 2,    /* Test runner (connects to sockets) */
};

/**
 * Initialize HIL subsystem
 * 
 * @param mode HIL mode (disabled, SUT, tester)
 * @param pid Process ID for socket naming (0 = use own PID)
 * @return 0 on success, negative errno on failure
 */
int lq_hil_init(enum lq_hil_mode mode, int pid);

/**
 * Cleanup HIL subsystem
 */
void lq_hil_cleanup(void);

/**
 * Check if HIL mode is active
 * 
 * @return true if HIL enabled
 */
bool lq_hil_is_active(void);

/* ============================================================================
 * SUT (System Under Test) API
 * 
 * These functions are called by the platform layer to receive injected data
 * ========================================================================== */

/**
 * Receive ADC injection (SUT side)
 * 
 * @param msg Output buffer for message
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return 0 on success, -EAGAIN if no data, negative errno on error
 */
int lq_hil_sut_recv_adc(struct lq_hil_adc_msg *msg, int timeout_ms);

/**
 * Receive SPI injection (SUT side)
 */
int lq_hil_sut_recv_spi(struct lq_hil_spi_msg *msg, int timeout_ms);

/**
 * Receive CAN injection (SUT side)
 */
int lq_hil_sut_recv_can(struct lq_hil_can_msg *msg, int timeout_ms);

/**
 * Send GPIO state change (SUT side - for output pins)
 * 
 * @param pin GPIO pin number
 * @param state Pin state (0 or 1)
 * @return 0 on success
 */
int lq_hil_sut_send_gpio(uint8_t pin, uint8_t state);

/**
 * Send CAN message (SUT side - for CAN output)
 */
int lq_hil_sut_send_can(const struct lq_hil_can_msg *msg);

/* ============================================================================
 * Tester API
 * 
 * These functions are called by test runner to inject inputs and verify outputs
 * ========================================================================== */

/**
 * Inject ADC sample (tester side)
 */
int lq_hil_tester_inject_adc(uint8_t channel, uint32_t value);

/**
 * Inject SPI data (tester side)
 */
int lq_hil_tester_inject_spi(uint8_t channel, const uint8_t *data, size_t len);

/**
 * Inject CAN message (tester side)
 */
int lq_hil_tester_inject_can(uint32_t can_id, bool is_extended,
                              const uint8_t *data, uint8_t dlc);

/**
 * Wait for GPIO state (tester side)
 * 
 * @param pin GPIO pin to monitor
 * @param expected_state Expected state (0 or 1)
 * @param timeout_ms Timeout
 * @return 0 if state matched, -ETIMEDOUT if timeout, negative errno on error
 */
int lq_hil_tester_wait_gpio(uint8_t pin, uint8_t expected_state, int timeout_ms);

/**
 * Wait for CAN message (tester side)
 * 
 * @param msg Output buffer
 * @param timeout_ms Timeout
 * @return 0 on success, -ETIMEDOUT if timeout
 */
int lq_hil_tester_wait_can(struct lq_hil_can_msg *msg, int timeout_ms);

/**
 * Get timestamp for latency measurement
 */
uint64_t lq_hil_get_timestamp_us(void);

#ifdef __cplusplus
}
#endif

#endif /* LQ_HIL_H */
