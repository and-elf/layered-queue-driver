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

/* Forward declaration for platform ops */
struct lq_hil_platform_ops;

/* HIL message types */
enum lq_hil_msg_type {
    LQ_HIL_MSG_ADC = 0,      /* ADC sample (input) */
    LQ_HIL_MSG_SPI = 1,      /* SPI transaction (input) */
    LQ_HIL_MSG_CAN = 2,      /* CAN frame (bidirectional) */
    LQ_HIL_MSG_GPIO = 3,     /* GPIO state (bidirectional) */
    LQ_HIL_MSG_SYNC = 4,     /* Synchronization */
    LQ_HIL_MSG_UART = 5,     /* UART/Serial data (output) */
    LQ_HIL_MSG_PWM = 6,      /* PWM output */
    LQ_HIL_MSG_SPI_OUT = 7,  /* SPI output */
    LQ_HIL_MSG_I2C = 8,      /* I2C transaction (output) */
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

/* UART/Serial output message */
struct lq_hil_uart_msg {
    struct lq_hil_msg_hdr hdr;
    uint8_t port;            /* UART port number (0-3) */
    uint16_t length;         /* Data length */
    uint8_t data[256];       /* Serial data */
} __attribute__((packed));

/* PWM output message */
struct lq_hil_pwm_msg {
    struct lq_hil_msg_hdr hdr;
    uint8_t channel;         /* PWM channel */
    uint16_t duty_cycle;     /* Duty cycle (0-10000 = 0-100.00%) */
    uint32_t frequency_hz;   /* Frequency in Hz */
} __attribute__((packed));

/* SPI output message */
struct lq_hil_spi_out_msg {
    struct lq_hil_msg_hdr hdr;
    uint8_t cs_pin;          /* Chip select pin */
    uint16_t length;         /* Data length */
    uint8_t data[256];       /* SPI data to send */
} __attribute__((packed));

/* I2C transaction message */
struct lq_hil_i2c_msg {
    struct lq_hil_msg_hdr hdr;
    uint8_t address;         /* I2C device address */
    uint8_t is_read;         /* 1 = read, 0 = write */
    uint16_t length;         /* Data length */
    uint8_t data[256];       /* I2C data */
} __attribute__((packed));

/* HIL socket paths (runtime with PID) */
#define LQ_HIL_SOCKET_ADC   "/tmp/lq_hil_adc_%d"
#define LQ_HIL_SOCKET_SPI   "/tmp/lq_hil_spi_%d"
#define LQ_HIL_SOCKET_CAN   "/tmp/lq_hil_can_%d"
#define LQ_HIL_SOCKET_GPIO  "/tmp/lq_hil_gpio_%d"
#define LQ_HIL_SOCKET_SYNC  "/tmp/lq_hil_sync_%d"
#define LQ_HIL_SOCKET_UART  "/tmp/lq_hil_uart_%d"
#define LQ_HIL_SOCKET_PWM   "/tmp/lq_hil_pwm_%d"
#define LQ_HIL_SOCKET_SPI_OUT "/tmp/lq_hil_spi_out_%d"
#define LQ_HIL_SOCKET_I2C   "/tmp/lq_hil_i2c_%d"

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
 * @param mode_str Optional mode string ("sut", "tester", "disabled") to override mode parameter, NULL to use mode parameter
 * @param pid Process ID for socket naming (0 = use own PID)
 * @return 0 on success, negative errno on failure
 */
int lq_hil_init(enum lq_hil_mode mode, const char *mode_str, int pid);

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
 * 
 * @param ops Platform operations (or NULL to use default)
 * @param msg Output buffer for message
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, -EAGAIN if no data, negative errno on error
 */
int lq_hil_sut_recv_can(const struct lq_hil_platform_ops *ops,
                         struct lq_hil_can_msg *msg, int timeout_ms);

/**
 * Send GPIO state change (SUT side - for output pins)
 * 
 * @param pin GPIO pin number
 * @param state Pin state (0 or 1)
 * @return 0 on success
 */
int lq_hil_sut_send_gpio(uint8_t pin, uint8_t state);

/**
 * Send UART/Serial data (SUT side)
 * 
 * @param port UART port number
 * @param data Data buffer
 * @param length Data length
 * @return 0 on success
 */
int lq_hil_sut_send_uart(uint8_t port, const uint8_t *data, uint16_t length);

/**
 * Send PWM output (SUT side)
 * 
 * @param channel PWM channel
 * @param duty_cycle Duty cycle (0-10000 = 0-100.00%)
 * @param frequency_hz Frequency in Hz
 * @return 0 on success
 */
int lq_hil_sut_send_pwm(uint8_t channel, uint16_t duty_cycle, uint32_t frequency_hz);

/**
 * Send SPI output (SUT side)
 * 
 * @param cs_pin Chip select pin
 * @param data Data buffer
 * @param length Data length
 * @return 0 on success
 */
int lq_hil_sut_send_spi_out(uint8_t cs_pin, const uint8_t *data, uint16_t length);

/**
 * Send I2C transaction (SUT side)
 * 
 * @param address I2C device address
 * @param is_read 1 for read, 0 for write
 * @param data Data buffer
 * @param length Data length
 * @return 0 on success
 */
int lq_hil_sut_send_i2c(uint8_t address, uint8_t is_read, const uint8_t *data, uint16_t length);

/**
 * Send CAN message (SUT side - for CAN output)
 * 
 * @param ops Platform operations (or NULL to use default)
 * @param msg CAN message to send
 * @return 0 on success, negative errno on error
 */
int lq_hil_sut_send_can(const struct lq_hil_platform_ops *ops,
                         const struct lq_hil_can_msg *msg);

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
 * @param ops Platform operations (or NULL to use default)
 * @param pin GPIO pin to monitor
 * @param expected_state Expected state (0 or 1)
 * @param timeout_ms Timeout
 * @return 0 if state matched, -ETIMEDOUT if timeout, negative errno on error
 */
int lq_hil_tester_wait_gpio(const struct lq_hil_platform_ops *ops,
                             uint8_t pin, uint8_t expected_state, int timeout_ms);

/**
 * Wait for CAN message (tester side)
 * 
 * @param msg Output buffer
 * @param timeout_ms Timeout
 * @return 0 on success, -ETIMEDOUT if timeout
 */
int lq_hil_tester_wait_can(struct lq_hil_can_msg *msg, int timeout_ms);

/**
 * Wait for UART/Serial output (tester side)
 * 
 * @param msg Output buffer for UART message
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, -EAGAIN on timeout
 */
int lq_hil_tester_wait_uart(struct lq_hil_uart_msg *msg, int timeout_ms);

/**
 * Wait for PWM output (tester side)
 * 
 * @param msg Output buffer for PWM message
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, -EAGAIN on timeout
 */
int lq_hil_tester_wait_pwm(struct lq_hil_pwm_msg *msg, int timeout_ms);

/**
 * Wait for SPI output (tester side)
 * 
 * @param msg Output buffer for SPI message
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, -EAGAIN on timeout
 */
int lq_hil_tester_wait_spi_out(struct lq_hil_spi_out_msg *msg, int timeout_ms);

/**
 * Wait for I2C transaction (tester side)
 * 
 * @param msg Output buffer for I2C message
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, -EAGAIN on timeout
 */
int lq_hil_tester_wait_i2c(struct lq_hil_i2c_msg *msg, int timeout_ms);

/**
 * Get timestamp for latency measurement
 */
uint64_t lq_hil_get_timestamp_us(void);

#ifdef __cplusplus
}
#endif

#endif /* LQ_HIL_H */
