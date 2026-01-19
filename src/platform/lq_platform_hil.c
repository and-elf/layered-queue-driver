/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * HIL platform implementation - intercepts hardware calls and routes through HIL messages
 */

#include "lq_platform.h"
#include "lq_hil.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdarg.h>
#ifndef __APPLE__
#include <semaphore.h>
#endif

/* ============================================================================
 * Time API
 * ============================================================================ */

uint32_t lq_platform_uptime_get(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

uint64_t lq_platform_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000);
}

void lq_platform_sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/* ============================================================================
 * Mutex API
 * ============================================================================ */

int lq_mutex_init(struct lq_mutex *mutex) {
    return pthread_mutex_init(&mutex->mutex, NULL);
}

int lq_mutex_lock(struct lq_mutex *mutex) {
    return pthread_mutex_lock(&mutex->mutex);
}

int lq_mutex_unlock(struct lq_mutex *mutex) {
    return pthread_mutex_unlock(&mutex->mutex);
}

void lq_mutex_destroy(struct lq_mutex *mutex) {
    pthread_mutex_destroy(&mutex->mutex);
}

/* ============================================================================
 * Semaphore API
 * ============================================================================ */

#ifdef __APPLE__
int lq_sem_init(struct lq_sem *sem, uint32_t initial, uint32_t max) {
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->count = initial;
    sem->max_count = max;
    return 0;
}

int lq_sem_take(struct lq_sem *sem, uint32_t timeout_ms) {
    pthread_mutex_lock(&sem->mutex);
    
    if (timeout_ms == 0xFFFFFFFF) {
        while (sem->count == 0) {
            pthread_cond_wait(&sem->cond, &sem->mutex);
        }
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        
        while (sem->count == 0) {
            int ret = pthread_cond_timedwait(&sem->cond, &sem->mutex, &ts);
            if (ret != 0) {
                pthread_mutex_unlock(&sem->mutex);
                return -EAGAIN;
            }
        }
    }
    
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

int lq_sem_give(struct lq_sem *sem) {
    pthread_mutex_lock(&sem->mutex);
    if (sem->count < sem->max_count) {
        sem->count++;
        pthread_cond_signal(&sem->cond);
    }
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

void lq_sem_destroy(struct lq_sem *sem) {
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
}
#else
int lq_sem_init(struct lq_sem *sem, uint32_t initial, uint32_t max) {
    (void)max;
    return sem_init(&sem->sem, 0, initial);
}

int lq_sem_take(struct lq_sem *sem, uint32_t timeout_ms) {
    if (timeout_ms == 0xFFFFFFFF) {
        return sem_wait(&sem->sem);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        return sem_timedwait(&sem->sem, &ts);
    }
}

int lq_sem_give(struct lq_sem *sem) {
    return sem_post(&sem->sem);
}

void lq_sem_destroy(struct lq_sem *sem) {
    sem_destroy(&sem->sem);
}
#endif

/* ============================================================================
 * Atomic API
 * ============================================================================ */

void lq_atomic_set(lq_atomic_t *target, int32_t value) {
    __atomic_store_n(&target->value, value, __ATOMIC_SEQ_CST);
}

int32_t lq_atomic_get(const lq_atomic_t *target) {
    return __atomic_load_n(&target->value, __ATOMIC_SEQ_CST);
}

int32_t lq_atomic_inc(lq_atomic_t *target) {
    return __atomic_fetch_add(&target->value, 1, __ATOMIC_SEQ_CST);
}

int32_t lq_atomic_dec(lq_atomic_t *target) {
    return __atomic_fetch_sub(&target->value, 1, __ATOMIC_SEQ_CST);
}

/* ============================================================================
 * Memory API
 * ============================================================================ */

void *lq_malloc(size_t size) {
    return malloc(size);
}

void lq_free(void *ptr) {
    free(ptr);
}

/* ============================================================================
 * Logging API
 * ============================================================================ */

void lq_log(lq_log_level_t level, const char *module, const char *fmt, ...) {
    const char *level_str[] = {"ERR", "WRN", "INF", "DBG"};
    
    printf("[%s] %s: ", level_str[level], module);
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\n");
}

/* ============================================================================
 * Hardware Abstraction - HIL Interceptors
 * ============================================================================ */

/*
 * These functions intercept hardware access and route through HIL messages.
 * This allows the real application to run in HIL mode without modification.
 */

/**
 * @brief Read ADC channel (HIL intercept)
 * 
 * In HIL mode, this receives ADC values from the HIL tester instead of
 * reading real hardware.
 */
int lq_adc_read(uint8_t channel, uint16_t *value) {
    if (!lq_hil_is_active()) {
        *value = 0;
        return -ENODEV;
    }
    
    struct lq_hil_adc_msg msg;
    
    /* Receive ADC injection from tester */
    if (lq_hil_sut_recv_adc(&msg, 100) != 0) {
        return -EAGAIN;
    }
    
    /* Verify channel matches */
    if (msg.hdr.channel != channel) {
        LQ_LOG_WRN("lq_adc", "Channel mismatch: expected %d, got %d", 
                   channel, msg.hdr.channel);
        return -EINVAL;
    }
    
    *value = msg.value;
    return 0;
}

/**
 * @brief Send CAN message (HIL intercept)
 *
 * In HIL mode, this sends CAN messages to the HIL tester for verification
 * instead of transmitting on real hardware.
 */
int lq_can_send(uint8_t device_index, uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }

    struct lq_hil_can_msg msg;

    msg.hdr.type = LQ_HIL_MSG_CAN;
    msg.hdr.timestamp_us = lq_platform_get_time_us();
    msg.hdr.channel = device_index;  /* CAN bus index */

    msg.can_id = can_id;
    msg.is_extended = is_extended;
    msg.dlc = len;
    memcpy(msg.data, data, len);

    return lq_hil_sut_send_can(NULL, &msg);
}

/**
 * @brief Receive CAN message (HIL intercept)
 *
 * In HIL mode, this receives CAN messages injected by the HIL tester.
 */
int lq_can_recv(uint8_t device_index, uint32_t *can_id, bool *is_extended, uint8_t *data, uint8_t *len, uint32_t timeout_ms) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }

    struct lq_hil_can_msg msg;

    if (lq_hil_sut_recv_can(NULL, &msg, timeout_ms) != 0) {
        return -EAGAIN;
    }

    /* Filter by device_index (channel) */
    if (msg.hdr.channel != device_index) {
        return -EAGAIN;
    }

    *can_id = msg.can_id;
    *is_extended = msg.is_extended;
    *len = msg.dlc;
    memcpy(data, msg.data, msg.dlc);

    return 0;
}

/**
 * @brief Set GPIO output (HIL intercept)
 * 
 * In HIL mode, this sends GPIO state to the HIL tester.
 */
int lq_gpio_set(uint8_t pin, bool value) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }
    
    return lq_hil_sut_send_gpio(pin, value ? 1 : 0);
}

/**
 * @brief Get GPIO input (HIL intercept)
 * 
 * In HIL mode, GPIO inputs would be injected via test scenarios.
 * For now, this is a placeholder.
 */
int lq_gpio_get(uint8_t pin, bool *value) {
    if (!lq_hil_is_active()) {
        *value = false;
        return -ENODEV;
    }
    
    /* TODO: Implement GPIO input injection in HIL test scenarios */
    LQ_LOG_WRN("lq_gpio", "GPIO input not yet implemented in HIL");
    *value = false;
    return -ENOTSUP;
}

/**
 * @brief Send UART data (HIL intercept)
 * 
 * In HIL mode, this sends UART data to the HIL tester for verification.
 */
int lq_uart_send(uint8_t port, const uint8_t *data, uint16_t length) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }
    
    return lq_hil_sut_send_uart(port, data, length);
}

/**
 * @brief Set PWM output (HIL intercept)
 * 
 * In HIL mode, this sends PWM settings to the HIL tester for verification.
 * 
 * @param channel PWM channel number
 * @param duty_cycle PWM duty cycle (0-10000 represents 0-100.00%)
 * @param frequency_hz PWM frequency in Hz
 */
int lq_pwm_set(uint8_t channel, uint16_t duty_cycle, uint32_t frequency_hz) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }
    
    return lq_hil_sut_send_pwm(channel, duty_cycle, frequency_hz);
}

/**
 * @brief Send SPI data (HIL intercept)
 * 
 * In HIL mode, this sends SPI output to the HIL tester for verification.
 * 
 * @param cs_pin Chip select pin number
 * @param data Data to send
 * @param length Length of data
 */
int lq_spi_send(uint8_t cs_pin, const uint8_t *data, uint16_t length) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }
    
    return lq_hil_sut_send_spi_out(cs_pin, data, length);
}

/**
 * @brief Write I2C data (HIL intercept)
 * 
 * In HIL mode, this sends I2C write transactions to the HIL tester for verification.
 * 
 * @param address I2C device address
 * @param data Data to write
 * @param length Length of data
 */
int lq_i2c_write(uint8_t address, const uint8_t *data, uint16_t length) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }
    
    return lq_hil_sut_send_i2c(address, 0, data, length);
}

/**
 * @brief Read I2C data (HIL intercept)
 * 
 * In HIL mode, this sends I2C read request to the HIL tester.
 * 
 * @param address I2C device address
 * @param data Buffer to store read data
 * @param length Length to read
 */
int lq_i2c_read(uint8_t address, uint8_t *data, uint16_t length) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }
    
    return lq_hil_sut_send_i2c(address, 1, data, length);
}
