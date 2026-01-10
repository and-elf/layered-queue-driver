/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform abstraction layer for layered queue driver.
 * Allows building and testing without Zephyr.
 */

#ifndef LQ_PLATFORM_H_
#define LQ_PLATFORM_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform detection */
#if defined(ARDUINO)
    #define LQ_PLATFORM_ARDUINO 1
#elif defined(__ZEPHYR__)
    #define LQ_PLATFORM_ZEPHYR 1
#else
    #define LQ_PLATFORM_NATIVE 1
#endif

/* ============================================================================
 * Time API
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 * @return Milliseconds since system start
 */
uint32_t lq_platform_uptime_get(void);

/**
 * @brief Get current time in microseconds
 * @return Microseconds since system start
 */
uint64_t lq_platform_get_time_us(void);

/**
 * @brief Sleep for specified milliseconds
 * @param ms Milliseconds to sleep
 */
void lq_platform_sleep_ms(uint32_t ms);

/**
 * @brief Delay for specified milliseconds (alias for sleep)
 * @param ms Milliseconds to delay
 */
static inline void lq_platform_delay_ms(uint32_t ms) {
    lq_platform_sleep_ms(ms);
}

/**
 * @brief Get tick count in milliseconds (alias for uptime)
 * Used by event crosscheck timeout tracking
 * @return Milliseconds since system start
 */
static inline uint32_t lq_get_tick_ms(void) {
    return lq_platform_uptime_get();
}

/**
 * @brief Start the engine task/thread or run the main loop
 * 
 * Platform-specific behavior:
 * - FreeRTOS: Creates task and starts scheduler (never returns)
 * - Zephyr: Creates thread (returns, threads continue)
 * - Native/Bare metal: Runs infinite loop (never returns)
 * 
 * @return 0 on success, negative on error
 */
int lq_engine_run(void);

/* ============================================================================
 * Mutex API
 * ============================================================================ */

#if defined(LQ_PLATFORM_ARDUINO)
/* Arduino - single threaded, no mutex/semaphore needed */
struct lq_mutex {
    int dummy;  /* Empty struct not allowed in C */
};

struct lq_sem {
    int dummy;  /* Empty struct not allowed in C */
};

typedef struct lq_mutex lq_mutex_t;
typedef struct lq_sem lq_sem_t;

#elif defined(LQ_PLATFORM_NATIVE)
#include <pthread.h>
#ifndef __APPLE__
#include <semaphore.h>
#endif

struct lq_mutex {
    pthread_mutex_t mutex;
};

#ifdef __APPLE__
/* macOS: Use mutex+cond instead of deprecated sem_init */
struct lq_sem {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint32_t count;
    uint32_t max_count;
};
#else
/* Other POSIX platforms: Use standard semaphores */
struct lq_sem {
    sem_t sem;
    uint32_t max_count;
};
#endif

#elif defined(__ZEPHYR__)
#include <zephyr/kernel.h>

struct lq_mutex {
    struct k_mutex mutex;
};

struct lq_sem {
    struct k_sem sem;
    uint32_t max_count;
};

typedef struct {
    atomic_t value;
} lq_atomic_t;

#else
/* Other platforms will define these */
struct lq_mutex;
struct lq_sem;
#endif

typedef struct lq_mutex lq_mutex_t;
typedef struct lq_sem lq_sem_t;

/**
 * @brief Initialize a mutex
 * @param mutex Pointer to mutex structure
 * @return 0 on success, negative errno on failure
 */
int lq_mutex_init(lq_mutex_t *mutex);

/**
 * @brief Destroy a mutex
 * @param mutex Pointer to mutex structure
 */
void lq_mutex_destroy(lq_mutex_t *mutex);

/* ============================================================================
 * Semaphore API
 * ============================================================================ */

/**
 * @brief Initialize a semaphore
 * @param mutex Pointer to mutex structure
 * @return 0 on success, negative errno on failure
 */
int lq_mutex_lock(lq_mutex_t *mutex);

/**
 * @brief Unlock a mutex
 * @param mutex Pointer to mutex structure
 * @return 0 on success, negative errno on failure
 */
int lq_mutex_unlock(lq_mutex_t *mutex);

/**
 * @brief Destroy a mutex
 * @param mutex Pointer to mutex structure
 */
void lq_mutex_destroy(lq_mutex_t *mutex);

/**
 * @brief Lock a mutex (blocking)
 * @param sem Pointer to semaphore structure
 * @param initial_count Initial count
 * @param max_count Maximum count
 * @return 0 on success, negative errno on failure
 */
int lq_sem_init(lq_sem_t *sem, uint32_t initial_count, uint32_t max_count);

/**
 * @brief Take (decrement) a semaphore
 * @param sem Pointer to semaphore structure
 * @param timeout_ms Timeout in milliseconds (0=no wait, UINT32_MAX=forever)
 * @return 0 on success, -EAGAIN on timeout, negative errno on error
 */
int lq_sem_take(lq_sem_t *sem, uint32_t timeout_ms);

/**
#ifndef __ZEPHYR__
typedef struct {
    volatile int32_t value;
} lq_atomic_t;
#endifn success, negative errno on failure
 */
int lq_sem_give(lq_sem_t *sem);

/**
 * @brief Destroy a semaphore
 * @param sem Pointer to semaphore structure
 */
void lq_sem_destroy(lq_sem_t *sem);

/* ============================================================================
 * Atomic API
 * ============================================================================ */

typedef struct {
    volatile int32_t value;
} lq_atomic_t;

/**
 * @brief Atomically set a value
 * @param target Pointer to atomic variable
 * @param value Value to set
 */
void lq_atomic_set(lq_atomic_t *target, int32_t value);

/**
 * @brief Atomically get a value
 * @param target Pointer to atomic variable
 * @return Current value
 */
int32_t lq_atomic_get(const lq_atomic_t *target);

/**
 * @brief Atomically increment a value
 * @param target Pointer to atomic variable
 * @return Previous value
 */
int32_t lq_atomic_inc(lq_atomic_t *target);

/**
 * @brief Atomically decrement a value
 * @param target Pointer to atomic variable
 * @return Previous value
 */
int32_t lq_atomic_dec(lq_atomic_t *target);

/* ============================================================================
 * Memory API
 * ============================================================================ */

/**
 * @brief Allocate memory
 * @param size Size in bytes
 * @return Pointer to allocated memory, NULL on failure
 */
void *lq_malloc(size_t size);

/**
 * @brief Free memory
 * @param ptr Pointer to memory to free
 */
void lq_free(void *ptr);

/* ============================================================================
 * Logging API
 * ============================================================================ */

typedef enum {
    LQ_LOG_LEVEL_ERR = 0,
    LQ_LOG_LEVEL_WRN = 1,
    LQ_LOG_LEVEL_INF = 2,
    LQ_LOG_LEVEL_DBG = 3,
} lq_log_level_t;

/**
 * @brief Log a message
 * @param level Log level
 * @param module Module name
 * @param fmt Format string (printf-style)
 * @param ... Arguments
 */
void lq_log(lq_log_level_t level, const char *module, const char *fmt, ...);

/* Convenience macros */
#define LQ_LOG_ERR(module, fmt, ...) lq_log(LQ_LOG_LEVEL_ERR, module, fmt, ##__VA_ARGS__)
#define LQ_LOG_WRN(module, fmt, ...) lq_log(LQ_LOG_LEVEL_WRN, module, fmt, ##__VA_ARGS__)
#define LQ_LOG_INF(module, fmt, ...) lq_log(LQ_LOG_LEVEL_INF, module, fmt, ##__VA_ARGS__)
#define LQ_LOG_DBG(module, fmt, ...) lq_log(LQ_LOG_LEVEL_DBG, module, fmt, ##__VA_ARGS__)

/* ============================================================================
 * Hardware Abstraction API
 * ============================================================================ */

/**
 * @brief Read ADC channel
 * @param channel ADC channel number
 * @param value Pointer to store ADC value (0-4095 for 12-bit)
 * @return 0 on success, negative errno on failure
 */
int lq_adc_read(uint8_t channel, uint16_t *value);

/**
 * @brief Send CAN message
 * @param can_id CAN identifier
 * @param is_extended true for 29-bit extended ID, false for 11-bit standard
 * @param data Message data bytes
 * @param len Data length (0-8)
 * @return 0 on success, negative errno on failure
 */
int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len);

/**
 * @brief Receive CAN message
 * @param can_id Pointer to store CAN identifier
 * @param is_extended Pointer to store ID type flag
 * @param data Buffer for message data (must be >=8 bytes)
 * @param len Pointer to store data length
 * @param timeout_ms Receive timeout in milliseconds (0xFFFFFFFF = forever)
 * @return 0 on success, negative errno on failure
 */
int lq_can_recv(uint32_t *can_id, bool *is_extended, uint8_t *data, uint8_t *len, uint32_t timeout_ms);

/**
 * @brief Set GPIO output
 * @param pin GPIO pin number
 * @param value true for high, false for low
 * @return 0 on success, negative errno on failure
 */
int lq_gpio_set(uint8_t pin, bool value);

/**
 * @brief Read GPIO input
 * @param pin GPIO pin number
 * @param value Pointer to store pin state
 * @return 0 on success, negative errno on failure
 */
int lq_gpio_get(uint8_t pin, bool *value);

/**
 * @brief Send UART data
 * @param port UART port number
 * @param data Data to send
 * @param length Length of data
 * @return 0 on success, negative errno on failure
 */
int lq_uart_send(uint8_t port, const uint8_t *data, uint16_t length);

/**
 * @brief Set PWM output
 * @param channel PWM channel number
 * @param duty_cycle Duty cycle (0-10000 represents 0-100.00%)
 * @param frequency_hz PWM frequency in Hz
 * @return 0 on success, negative errno on failure
 */
int lq_pwm_set(uint8_t channel, uint16_t duty_cycle, uint32_t frequency_hz);

/**
 * @brief Send SPI data
 * @param cs_pin Chip select pin
 * @param data Data to send
 * @param length Length of data
 * @return 0 on success, negative errno on failure
 */
int lq_spi_send(uint8_t cs_pin, const uint8_t *data, uint16_t length);

/**
 * @brief Write I2C data
 * @param address I2C device address
 * @param data Data to write
 * @param length Length of data
 * @return 0 on success, negative errno on failure
 */
int lq_i2c_write(uint8_t address, const uint8_t *data, uint16_t length);

/**
 * @brief Read I2C data
 * @param address I2C device address
 * @param data Buffer to store read data
 * @param length Length to read
 * @return 0 on success, negative errno on failure
 */
int lq_i2c_read(uint8_t address, uint8_t *data, uint16_t length);

/**
 * @brief Toggle GPIO output
 * @param pin GPIO pin number
 * @return 0 on success, negative errno on failure
 */
int lq_gpio_toggle(uint8_t pin);

/**
 * @brief Receive UART data
 * @param port UART port number
 * @param data Buffer to store received data
 * @param length Maximum length to receive
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes received, or negative errno on failure
 */
int lq_uart_recv(uint8_t port, uint8_t *data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief Receive SPI data
 * @param cs_pin Chip select pin
 * @param data Buffer to store received data
 * @param length Length of data to receive
 * @return Number of bytes received, or negative errno on failure
 */
int lq_spi_recv(uint8_t cs_pin, uint8_t *data, uint16_t length);

/**
 * @brief Transceive SPI data (simultaneous send/receive)
 * @param cs_pin Chip select pin
 * @param tx_data Data to send
 * @param rx_data Buffer to store received data
 * @param length Length of transaction
 * @return 0 on success, negative errno on failure
 */
int lq_spi_transceive(uint8_t cs_pin, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length);

/**
 * @brief I2C write then read
 * @param address I2C device address
 * @param write_data Data to write
 * @param write_length Length of write data
 * @param read_data Buffer for read data
 * @param read_length Length to read
 * @return 0 on success, negative errno on failure
 */
int lq_i2c_write_read(uint8_t address, 
                      const uint8_t *write_data, uint16_t write_length,
                      uint8_t *read_data, uint16_t read_length);

/**
 * @brief Write byte to I2C register
 * @param address I2C device address
 * @param reg Register address
 * @param value Value to write
 * @return 0 on success, negative errno on failure
 */
int lq_i2c_reg_write_byte(uint8_t address, uint8_t reg, uint8_t value);

/**
 * @brief Read byte from I2C register
 * @param address I2C device address
 * @param reg Register address
 * @param value Pointer to store value
 * @return 0 on success, negative errno on failure
 */
int lq_i2c_reg_read_byte(uint8_t address, uint8_t reg, uint8_t *value);

/**
 * @brief Burst write to I2C registers
 * @param address I2C device address
 * @param start_reg Starting register address
 * @param data Data to write
 * @param length Length of data
 * @return 0 on success, negative errno on failure
 */
int lq_i2c_burst_write(uint8_t address, uint8_t start_reg, const uint8_t *data, uint16_t length);

/**
 * @brief Burst read from I2C registers
 * @param address I2C device address
 * @param start_reg Starting register address
 * @param data Buffer for read data
 * @param length Length to read
 * @return 0 on success, negative errno on failure
 */
int lq_i2c_burst_read(uint8_t address, uint8_t start_reg, uint8_t *data, uint16_t length);

/**
 * @brief Set default I2C bus
 * @param bus_id I2C bus number
 * @return 0 on success, negative errno on failure
 */
int lq_i2c_set_default_bus(uint8_t bus_id);

/**
 * @brief Write value to DAC
 * @param channel DAC channel number
 * @param value DAC value (typically 0-4095 for 12-bit)
 * @return 0 on success, negative errno on failure
 */
int lq_dac_write(uint8_t channel, uint16_t value);

/**
 * @brief Write Modbus register
 * @param slave_id Modbus slave ID
 * @param reg Register address
 * @param value Value to write
 * @return 0 on success, negative errno on failure
 */
int lq_modbus_write(uint8_t slave_id, uint16_t reg, uint16_t value);

/* ============================================================================
 * BLDC Motor Control
 * ============================================================================ */

/* Forward declaration */
struct lq_bldc_config;

/* Platform capability flags */
#if defined(STM32F4) || defined(STM32F7) || defined(STM32H7) || defined(STM32G4) || defined(ARDUINO_ARCH_STM32)
    #define LQ_PLATFORM_HAS_COMPLEMENTARY_PWM 1
    #define LQ_PLATFORM_HAS_DEADTIME 1
#elif defined(ESP32) || defined(ESP32S3) || defined(ESP_PLATFORM)
    #define LQ_PLATFORM_HAS_COMPLEMENTARY_PWM 1
    #define LQ_PLATFORM_HAS_DEADTIME 1
#elif defined(__SAMD21__) || defined(__SAMD51__) || defined(ARDUINO_ARCH_SAMD) || defined(__SAMD21G18A__) || defined(__SAMD51P19A__)
    #define LQ_PLATFORM_HAS_COMPLEMENTARY_PWM 1
    #define LQ_PLATFORM_HAS_DEADTIME 1
#elif defined(NRF52) || defined(NRF53)
    #define LQ_PLATFORM_HAS_COMPLEMENTARY_PWM 0
    #define LQ_PLATFORM_HAS_DEADTIME 0
#else
    /* Native/test platform - stub implementation */
    #define LQ_PLATFORM_HAS_COMPLEMENTARY_PWM 0
    #define LQ_PLATFORM_HAS_DEADTIME 0
#endif

/**
 * @brief Initialize BLDC motor PWM hardware
 * @param motor_id Motor instance ID
 * @param config Complete motor configuration including pins
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config);

/**
 * @brief Set PWM duty cycle for a motor phase
 * @param motor_id Motor instance ID
 * @param phase Phase number (0 to num_phases-1)
 * @param duty_cycle Duty cycle in 0.01% units (0-10000 = 0-100.00%)
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase, uint16_t duty_cycle);

/**
 * @brief Enable/disable motor PWM outputs
 * @param motor_id Motor instance ID
 * @param enable true to enable, false to disable
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_platform_enable(uint8_t motor_id, bool enable);

/**
 * @brief Emergency brake motor
 * @param motor_id Motor instance ID
 * @return 0 on success, negative errno on failure
 */
int lq_bldc_platform_brake(uint8_t motor_id);

/* ============================================================================ * Error codes (POSIX-compatible)
 * ============================================================================ */

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef ENOMEM
#define ENOMEM 12
#endif

#ifndef EAGAIN
#define EAGAIN 11
#endif

#ifndef ENODATA
#define ENODATA 61
#endif

#ifndef ENOTSUP
#define ENOTSUP 95
#endif

#ifndef ENODEV
#define ENODEV 19
#endif

#ifdef __cplusplus
}
#endif

#endif /* LQ_PLATFORM_H_ */
