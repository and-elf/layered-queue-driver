/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP32 Platform Implementation
 * 
 * Hardware: ESP32, ESP32-S3
 * BLDC Motor Control: MCPWM peripheral (Motor Control PWM)
 */

#include "lq_platform.h"
#include "driver/mcpwm.h"
#include "driver/gpio.h"
#include "soc/mcpwm_periph.h"

/* MCPWM unit assignment per motor (ESP32 has 2 units, ESP32-S3 has 1) */
static mcpwm_unit_t get_mcpwm_unit(uint8_t motor_id)
{
#if defined(CONFIG_IDF_TARGET_ESP32)
    /* ESP32 has MCPWM0 and MCPWM1 */
    return (motor_id == 0) ? MCPWM_UNIT_0 : MCPWM_UNIT_1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    /* ESP32-S3 has only MCPWM0 */
    (void)motor_id;
    return MCPWM_UNIT_0;
#else
    (void)motor_id;
    return MCPWM_UNIT_0;
#endif
}

/**
 * @brief Initialize MCPWM for 3-phase complementary PWM
 * 
 * @param motor_id Motor instance ID
 * @param config Complete motor configuration including pins
 * @return 0 on success, -1 on error
 */
static int mcpwm_init_3phase(uint8_t motor_id, const struct lq_bldc_config *config)
{
    mcpwm_unit_t unit = get_mcpwm_unit(motor_id);
    
    /* Configure GPIO pins from config */
    for (uint8_t phase = 0; phase < config->num_phases; phase++) {
        const struct lq_bldc_pin *hs_pin = &config->high_side_pins[phase];
        const struct lq_bldc_pin *ls_pin = &config->low_side_pins[phase];
        
        /* Calculate actual GPIO pin number from port and pin */
        int hs_gpio = (hs_pin->gpio_port * 32) + hs_pin->gpio_pin;
        int ls_gpio = (ls_pin->gpio_port * 32) + ls_pin->gpio_pin;
        
        /* Map phase to MCPWM operator and timer */
        mcpwm_timer_t timer;
        mcpwm_io_signals_t pwm_a, pwm_b;
        
        switch (phase) {
            case 0:  /* Phase U */
                timer = MCPWM_TIMER_0;
                pwm_a = MCPWM0A;
                pwm_b = MCPWM0B;
                break;
            case 1:  /* Phase V */
                timer = MCPWM_TIMER_1;
                pwm_a = MCPWM1A;
                pwm_b = MCPWM1B;
                break;
            case 2:  /* Phase W */
                timer = MCPWM_TIMER_2;
                pwm_a = MCPWM2A;
                pwm_b = MCPWM2B;
                break;
            default:
                return -22;  /* EINVAL */
        }
        
        /* Set GPIO for MCPWM outputs */
        mcpwm_gpio_init(unit, pwm_a, hs_gpio);
        mcpwm_gpio_init(unit, pwm_b, ls_gpio);
    }
    
    /* Configure MCPWM with center-aligned mode */
    mcpwm_config_t pwm_config = {
        .frequency = config->pwm_frequency_hz,
        .cmpr_a = 0,  /* Initially 0% duty */
        .cmpr_b = 0,
        .duty_mode = MCPWM_DUTY_MODE_0,
        .counter_mode = MCPWM_UP_DOWN_COUNTER,  /* Center-aligned */
    };
    
    /* Initialize all 3 timers */
    for (uint8_t phase = 0; phase < config->num_phases; phase++) {
        mcpwm_timer_t timer = (mcpwm_timer_t)phase;
        
        if (mcpwm_init(unit, timer, &pwm_config) != ESP_OK) {
            return -1;
        }
        
        /* Configure deadtime if enabled */
        if (config->enable_deadtime) {
            /* Convert nanoseconds to MCPWM clock cycles (160MHz) */
            uint32_t dt_cycles = (config->deadtime_ns * 160U) / 1000U;
            
            mcpwm_deadtime_type_t dt_type = MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE;
            
            if (mcpwm_deadtime_enable(unit, timer, dt_type, dt_cycles, dt_cycles) != ESP_OK) {
                return -1;
            }
        }
        
        /* Set initial duty to 0 */
        mcpwm_set_duty(unit, timer, MCPWM_OPR_A, 0);
        mcpwm_set_duty(unit, timer, MCPWM_OPR_B, 0);
        mcpwm_set_duty_type(unit, timer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
        mcpwm_set_duty_type(unit, timer, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
    }
    
    return 0;
}

/* =============================================================================
 * BLDC Motor Control Platform Functions
 * ========================================================================== */

int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config)
{
    if (config->num_phases != 3) {
        return -95;  /* ENOTSUP - only 3-phase supported */
    }
    
#if defined(CONFIG_IDF_TARGET_ESP32)
    if (motor_id >= 2) {
        return -22;  /* EINVAL - ESP32 has only 2 MCPWM units */
    }
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    if (motor_id >= 1) {
        return -22;  /* EINVAL - ESP32-S3 has only 1 MCPWM unit */
    }
#endif
    
    return mcpwm_init_3phase(motor_id, config);
}

int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase, uint16_t duty_cycle)
{
    if (phase >= 3) {
        return -22;  /* EINVAL */
    }
    
    mcpwm_unit_t unit = get_mcpwm_unit(motor_id);
    mcpwm_timer_t timer = (mcpwm_timer_t)phase;
    
    /* Convert duty_cycle (0-10000 = 0-100.00%) to percentage (0-100) */
    float duty_percent = (float)duty_cycle / 100.0f;
    
    /* In complementary mode, PWM_A is the duty cycle, PWM_B is complement */
    mcpwm_set_duty(unit, timer, MCPWM_OPR_A, duty_percent);
    
    return 0;
}

int lq_bldc_platform_enable(uint8_t motor_id, bool enable)
{
    mcpwm_unit_t unit = get_mcpwm_unit(motor_id);
    
    if (enable) {
        /* Start all 3 timers */
        for (uint8_t phase = 0; phase < 3; phase++) {
            mcpwm_timer_t timer = (mcpwm_timer_t)phase;
            mcpwm_start(unit, timer);
        }
    } else {
        /* Stop all 3 timers */
        for (uint8_t phase = 0; phase < 3; phase++) {
            mcpwm_timer_t timer = (mcpwm_timer_t)phase;
            mcpwm_stop(unit, timer);
        }
    }
    
    return 0;
}

int lq_bldc_platform_brake(uint8_t motor_id)
{
    mcpwm_unit_t unit = get_mcpwm_unit(motor_id);
    
    /* Set all phases to 0% duty (low-side active) and stop */
    for (uint8_t phase = 0; phase < 3; phase++) {
        mcpwm_timer_t timer = (mcpwm_timer_t)phase;
        mcpwm_set_duty(unit, timer, MCPWM_OPR_A, 0);
        mcpwm_set_duty(unit, timer, MCPWM_OPR_B, 0);
        mcpwm_stop(unit, timer);
    }
    
    return 0;
}

/* =============================================================================
 * UART Platform Functions (ESP-IDF)
 * ========================================================================== */

#include "driver/uart.h"

int lq_uart_send(uint8_t port, const uint8_t *data, uint16_t length)
{
    if (port >= UART_NUM_MAX) {
        return -ENODEV;
    }
    
    int written = uart_write_bytes(port, (const char*)data, length);
    return (written == length) ? 0 : -EIO;
}

int lq_uart_recv(uint8_t port, uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    if (port >= UART_NUM_MAX) {
        return -ENODEV;
    }
    
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    int read = uart_read_bytes(port, data, length, ticks);
    
    if (read == length) {
        return length;
    } else if (read >= 0) {
        return -ETIMEDOUT;
    }
    return -EIO;
}

/* =============================================================================
 * GPIO Platform Functions (ESP-IDF)
 * ========================================================================== */

int lq_gpio_set(uint8_t pin, bool value)
{
    return gpio_set_level((gpio_num_t)pin, value ? 1 : 0);
}

int lq_gpio_get(uint8_t pin, bool *value)
{
    if (!value) {
        return -EINVAL;
    }
    
    int level = gpio_get_level((gpio_num_t)pin);
    *value = (level != 0);
    return 0;
}

int lq_gpio_toggle(uint8_t pin)
{
    bool current;
    int ret = lq_gpio_get(pin, &current);
    if (ret != 0) return ret;
    
    return lq_gpio_set(pin, !current);
}

/* =============================================================================
 * PWM Platform Functions (ESP-IDF LEDC)
 * Use LEDC peripheral for general PWM (MCPWM is reserved for BLDC motors)
 * ========================================================================== */

#include "driver/ledc.h"

/* LEDC channel configuration - statically allocated */
#define MAX_PWM_CHANNELS 8
static struct {
    ledc_channel_t channel;
    ledc_timer_t timer;
    bool configured;
} pwm_channels[MAX_PWM_CHANNELS];

int lq_pwm_set(uint8_t channel, uint16_t duty_cycle, uint32_t frequency_hz)
{
    if (channel >= MAX_PWM_CHANNELS) {
        return -ENODEV;
    }
    
    /* Configure channel if first use */
    if (!pwm_channels[channel].configured) {
        ledc_timer_config_t timer_conf = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = (ledc_timer_t)(channel / 2),  /* 2 channels per timer */
            .duty_resolution = LEDC_TIMER_13_BIT,
            .freq_hz = (frequency_hz > 0) ? frequency_hz : 1000,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ledc_timer_config(&timer_conf);
        
        ledc_channel_config_t chan_conf = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)channel,
            .timer_sel = (ledc_timer_t)(channel / 2),
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = -1,  /* User must configure GPIO separately */
            .duty = 0,
            .hpoint = 0
        };
        ledc_channel_config(&chan_conf);
        
        pwm_channels[channel].channel = (ledc_channel_t)channel;
        pwm_channels[channel].timer = (ledc_timer_t)(channel / 2);
        pwm_channels[channel].configured = true;
    }
    
    /* Update frequency if provided */
    if (frequency_hz > 0) {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, pwm_channels[channel].timer, frequency_hz);
    }
    
    /* Convert duty_cycle (0-10000 = 0-100.00%) to 13-bit value (0-8191) */
    uint32_t duty = ((uint32_t)duty_cycle * 8191) / 10000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, pwm_channels[channel].channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, pwm_channels[channel].channel);
    
    return 0;
}

/* =============================================================================
 * SPI Platform Functions (ESP-IDF)
 * ========================================================================== */

#include "driver/spi_master.h"

static spi_device_handle_t spi_devices[8] = {NULL};

int lq_spi_send(uint8_t cs_pin, const uint8_t *data, uint16_t length)
{
    if (cs_pin >= 8 || !spi_devices[cs_pin]) {
        return -ENODEV;
    }
    
    spi_transaction_t trans = {
        .length = length * 8,  /* Length in bits */
        .tx_buffer = data,
        .rx_buffer = NULL
    };
    
    esp_err_t ret = spi_device_transmit(spi_devices[cs_pin], &trans);
    return (ret == ESP_OK) ? 0 : -EIO;
}

int lq_spi_recv(uint8_t cs_pin, uint8_t *data, uint16_t length)
{
    if (cs_pin >= 8 || !spi_devices[cs_pin]) {
        return -ENODEV;
    }
    
    spi_transaction_t trans = {
        .length = length * 8,
        .tx_buffer = NULL,
        .rx_buffer = data
    };
    
    esp_err_t ret = spi_device_transmit(spi_devices[cs_pin], &trans);
    return (ret == ESP_OK) ? length : -EIO;
}

int lq_spi_transceive(uint8_t cs_pin, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    if (cs_pin >= 8 || !spi_devices[cs_pin]) {
        return -ENODEV;
    }
    
    spi_transaction_t trans = {
        .length = length * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data
    };
    
    esp_err_t ret = spi_device_transmit(spi_devices[cs_pin], &trans);
    return (ret == ESP_OK) ? 0 : -EIO;
}

/* =============================================================================
 * I2C Platform Functions (ESP-IDF)
 * ========================================================================== */

#include "driver/i2c.h"

static i2c_port_t default_i2c_port = I2C_NUM_0;

int lq_i2c_set_default_bus(uint8_t bus_id)
{
    if (bus_id >= I2C_NUM_MAX) {
        return -ENODEV;
    }
    default_i2c_port = (i2c_port_t)bus_id;
    return 0;
}

int lq_i2c_write(uint8_t address, const uint8_t *data, uint16_t length)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, length, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(default_i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? 0 : -EIO;
}

int lq_i2c_read(uint8_t address, uint8_t *data, uint16_t length)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
    
    if (length > 1) {
        i2c_master_read(cmd, data, length - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[length - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(default_i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? 0 : -EIO;
}

int lq_i2c_write_read(uint8_t address, 
                      const uint8_t *write_data, uint16_t write_length,
                      uint8_t *read_data, uint16_t read_length)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    /* Write phase */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_data, write_length, true);
    
    /* Read phase with repeated start */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
    if (read_length > 1) {
        i2c_master_read(cmd, read_data, read_length - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &read_data[read_length - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(default_i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? 0 : -EIO;
}

int lq_i2c_reg_write_byte(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return lq_i2c_write(address, data, 2);
}

int lq_i2c_reg_read_byte(uint8_t address, uint8_t reg, uint8_t *value)
{
    return lq_i2c_write_read(address, &reg, 1, value, 1);
}

int lq_i2c_burst_write(uint8_t address, uint8_t start_reg, const uint8_t *data, uint16_t length)
{
    /* Create buffer with register + data */
    uint8_t *buf = malloc(length + 1);
    if (!buf) return -ENOMEM;
    
    buf[0] = start_reg;
    memcpy(&buf[1], data, length);
    
    int ret = lq_i2c_write(address, buf, length + 1);
    free(buf);
    
    return ret;
}

int lq_i2c_burst_read(uint8_t address, uint8_t start_reg, uint8_t *data, uint16_t length)
{
    return lq_i2c_write_read(address, &start_reg, 1, data, length);
}
