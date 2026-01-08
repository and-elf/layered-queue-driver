/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Devicetree parsing macros for auto-generation
 * 
 * This header provides macros to parse devicetree nodes into C struct
 * initializers for the layered queue engine. It enables complete code
 * generation from DTS specifications.
 */

#ifndef LQ_DEVICETREE_H_
#define LQ_DEVICETREE_H_

#include <zephyr/devicetree.h>
#include "lq_engine.h"
#include "lq_hw_input.h"
#include "lq_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Hardware Input Parsing
 * ========================================================================== */

/**
 * @brief Get signal ID from hardware input node
 */
#define LQ_HW_INPUT_SIGNAL_ID(node_id) \
    DT_PROP(node_id, signal_id)

/**
 * @brief Get staleness timeout from hardware input node
 */
#define LQ_HW_INPUT_STALE_US(node_id) \
    DT_PROP(node_id, stale_us)

/**
 * @brief Get ADC channel from hardware ADC input node
 */
#define LQ_HW_ADC_CHANNEL(node_id) \
    DT_PROP(node_id, adc_channel)

/**
 * @brief Get ISR priority from hardware input node
 */
#define LQ_HW_INPUT_ISR_PRIORITY(node_id) \
    DT_PROP(node_id, isr_priority)

/**
 * @brief Get init priority from hardware input node
 */
#define LQ_HW_INPUT_INIT_PRIORITY(node_id) \
    DT_PROP_OR(node_id, init_priority, 80)

/* ============================================================================
 * Sensor Input Parsing
 * ========================================================================== */

/**
 * @brief Get sensor device from sensor input node
 */
#define LQ_SENSOR_DEVICE(node_id) \
    DEVICE_DT_GET(DT_PHANDLE(node_id, sensor_device))

/**
 * @brief Get sensor channel from sensor input node
 */
#define LQ_SENSOR_CHANNEL(node_id) \
    DT_PROP(node_id, sensor_channel)

/**
 * @brief Get scale factor from sensor input node
 */
#define LQ_SENSOR_SCALE_FACTOR(node_id) \
    DT_PROP_OR(node_id, scale_factor, 1000)

/**
 * @brief Get offset from sensor input node
 */
#define LQ_SENSOR_OFFSET(node_id) \
    DT_PROP_OR(node_id, offset, 0)

/**
 * @brief Get trigger type from sensor input node
 */
#define LQ_SENSOR_TRIGGER_TYPE(node_id) \
    DT_STRING_TOKEN(node_id, trigger_type)

/**
 * @brief Get poll interval from sensor input node
 */
#define LQ_SENSOR_POLL_INTERVAL_MS(node_id) \
    DT_PROP_OR(node_id, poll_interval_ms, 100)

/* ============================================================================
 * Mid-Level Merge Parsing
 * ========================================================================== */

/**
 * @brief Parse voting method string to enum
 */
#define LQ_DT_VOTING_METHOD(node_id) \
    (DT_STRING_TOKEN(node_id, voting_method) == median ? LQ_VOTE_MEDIAN : \
     DT_STRING_TOKEN(node_id, voting_method) == average ? LQ_VOTE_AVERAGE : \
     DT_STRING_TOKEN(node_id, voting_method) == min ? LQ_VOTE_MIN : \
     LQ_VOTE_MAX)

/**
 * @brief Generate lq_merge_ctx initializer from DTS node
 */
#define LQ_MERGE_CTX_INIT(node_id, idx) \
    [idx] = { \
        .output_signal = DT_PROP(node_id, output_signal_id), \
        .input_signals = DT_PROP(node_id, input_signal_ids), \
        .num_inputs = DT_PROP_LEN(node_id, input_signal_ids), \
        .voting_method = LQ_DT_VOTING_METHOD(node_id), \
        .tolerance = DT_PROP(node_id, tolerance), \
        .stale_us = DT_PROP_OR(node_id, stale_us, 0), \
    }

/* ============================================================================
 * Cyclic Output Parsing
 * ========================================================================== */

/**
 * @brief Parse output type string to enum
 */
#define LQ_DT_OUTPUT_TYPE(node_id) \
    (DT_STRING_TOKEN(node_id, output_type) == can ? LQ_OUTPUT_CAN : \
     DT_STRING_TOKEN(node_id, output_type) == j1939 ? LQ_OUTPUT_J1939 : \
     DT_STRING_TOKEN(node_id, output_type) == canopen ? LQ_OUTPUT_CANOPEN : \
     DT_STRING_TOKEN(node_id, output_type) == gpio ? LQ_OUTPUT_GPIO : \
     DT_STRING_TOKEN(node_id, output_type) == uart ? LQ_OUTPUT_UART : \
     DT_STRING_TOKEN(node_id, output_type) == spi ? LQ_OUTPUT_SPI : \
     DT_STRING_TOKEN(node_id, output_type) == i2c ? LQ_OUTPUT_I2C : \
     DT_STRING_TOKEN(node_id, output_type) == pwm ? LQ_OUTPUT_PWM : \
     DT_STRING_TOKEN(node_id, output_type) == dac ? LQ_OUTPUT_DAC : \
     DT_STRING_TOKEN(node_id, output_type) == modbus ? LQ_OUTPUT_MODBUS : \
     LQ_OUTPUT_CAN)

/**
 * @brief Generate lq_cyclic_ctx initializer from DTS node
 */
#define LQ_CYCLIC_CTX_INIT(node_id, idx) \
    [idx] = { \
        .signal_id = DT_PROP(node_id, source_signal_id), \
        .output_type = LQ_DT_OUTPUT_TYPE(node_id), \
        .target_id = DT_PROP(node_id, target_id), \
        .period_us = DT_PROP(node_id, period_us), \
        .next_deadline_us = DT_PROP_OR(node_id, deadline_offset_us, 0), \
        .priority = DT_PROP_OR(node_id, priority, 7), \
    }

/* ============================================================================
 * Engine Initialization
 * ========================================================================== */

/**
 * @brief Count compatible nodes (for array sizing)
 */
#define LQ_DT_NUM_COMPAT(compat) DT_NUM_INST_STATUS_OKAY(compat)

/**
 * @brief Foreach macro for merge contexts
 * 
 * Generates array initializers for all lq,mid-merge nodes
 */
#define LQ_FOREACH_MERGE(fn) \
    DT_FOREACH_STATUS_OKAY(lq_mid_merge, fn)

/**
 * @brief Foreach macro for cyclic outputs
 * 
 * Generates array initializers for all lq,cyclic-output nodes
 */
#define LQ_FOREACH_CYCLIC_OUTPUT(fn) \
    DT_FOREACH_STATUS_OKAY(lq_cyclic_output, fn)

/**
 * @brief Foreach macro for hardware ADC inputs
 * 
 * Generates ISR handlers and signal initializers
 */
#define LQ_FOREACH_HW_ADC_INPUT(fn) \
    DT_FOREACH_STATUS_OKAY(lq_hw_adc_input, fn)

/**
 * @brief Foreach macro for hardware SPI inputs
 * 
 * Generates ISR handlers and signal initializers
 */
#define LQ_FOREACH_HW_SPI_INPUT(fn) \
    DT_FOREACH_STATUS_OKAY(lq_hw_spi_input, fn)

/**
 * @brief Foreach macro for hardware sensor inputs
 * 
 * Generates trigger callbacks and signal initializers
 */
#define LQ_FOREACH_HW_SENSOR_INPUT(fn) \
    DT_FOREACH_STATUS_OKAY(lq_hw_sensor_input, fn)

/* ============================================================================
 * ISR Generation Helpers
 * ========================================================================== */

/**
 * @brief Generate ISR handler for ADC input
 * 
 * Creates a function that:
 * 1. Reads ADC value
 * 2. Calls lq_hw_push()
 * 3. Wakes engine task
 */
#define LQ_GEN_ADC_ISR_HANDLER(node_id) \
    static void lq_adc_isr_##node_id(const struct device *dev, \
                                      const struct adc_sequence *sequence, \
                                      uint16_t sampling_index) \
    { \
        int32_t value = (int32_t)sequence->buffer; \
        lq_hw_push(LQ_HW_INPUT_SIGNAL_ID(node_id), value, lq_platform_get_time_us()); \
    }

/**
 * @brief Generate ISR configuration for ADC input
 * 
 * Sets up ADC channel and interrupt
 */
#define LQ_GEN_ADC_ISR_SETUP(node_id) \
    do { \
        const struct device *adc_dev = DEVICE_DT_GET(DT_PHANDLE(node_id, adc_device)); \
        struct adc_channel_cfg channel_cfg = { \
            .channel_id = LQ_HW_ADC_CHANNEL(node_id), \
            .gain = ADC_GAIN_1, \
            .reference = ADC_REF_INTERNAL, \
            .acquisition_time = ADC_ACQ_TIME_DEFAULT, \
        }; \
        adc_channel_setup(adc_dev, &channel_cfg); \
    } while (0)

/**
 * @brief Generate ISR handler for SPI input
 * 
 * Creates a GPIO interrupt callback that:
 * 1. Reads SPI sensor
 * 2. Calls lq_hw_push()
 * 3. Wakes engine task
 */
#define LQ_GEN_SPI_ISR_HANDLER(node_id) \
    static void lq_spi_isr_##node_id(const struct device *dev, \
                                      struct gpio_callback *cb, \
                                      uint32_t pins) \
    { \
        const struct device *spi_dev = DEVICE_DT_GET(DT_PHANDLE(node_id, spi_device)); \
        uint8_t rx_buffer[4]; \
        struct spi_buf rx_buf = {.buf = rx_buffer, .len = DT_PROP(node_id, num_bytes)}; \
        struct spi_buf_set rx_bufs = {.buffers = &rx_buf, .count = 1}; \
        \
        if (spi_read(spi_dev, NULL, &rx_bufs) == 0) { \
            int32_t value = lq_spi_bytes_to_value(rx_buffer, DT_PROP(node_id, num_bytes), \
                                                   DT_PROP(node_id, signed)); \
            lq_hw_push(LQ_HW_INPUT_SIGNAL_ID(node_id), value, lq_platform_get_time_us()); \
        } \
    }

/**
 * @brief Generate trigger handler for sensor input
 * 
 * Creates a sensor trigger callback that:
 * 1. Fetches sensor data
 * 2. Converts to int32_t with scaling
 * 3. Calls lq_hw_push()
 */
#define LQ_GEN_SENSOR_TRIGGER_HANDLER(node_id) \
    static void lq_sensor_trigger_##node_id(const struct device *dev, \
                                             const struct sensor_trigger *trig) \
    { \
        struct sensor_value val; \
        int ret = sensor_sample_fetch(dev); \
        if (ret == 0) { \
            ret = sensor_channel_get(dev, LQ_SENSOR_CHANNEL(node_id), &val); \
            if (ret == 0) { \
                /* Convert sensor_value to int32_t with scaling */ \
                int32_t value = (val.val1 * LQ_SENSOR_SCALE_FACTOR(node_id)) + \
                               ((val.val2 * LQ_SENSOR_SCALE_FACTOR(node_id)) / 1000000) + \
                               LQ_SENSOR_OFFSET(node_id); \
                lq_hw_push(LQ_HW_INPUT_SIGNAL_ID(node_id), value, lq_platform_get_time_us()); \
            } \
        } \
    }

/**
 * @brief Generate sensor trigger setup
 * 
 * Configures sensor trigger for data-ready interrupts
 */
#define LQ_GEN_SENSOR_TRIGGER_SETUP(node_id) \
    do { \
        const struct device *sensor_dev = LQ_SENSOR_DEVICE(node_id); \
        if (device_is_ready(sensor_dev)) { \
            struct sensor_trigger trig = { \
                .type = SENSOR_TRIG_DATA_READY, \
                .chan = LQ_SENSOR_CHANNEL(node_id), \
            }; \
            sensor_trigger_set(sensor_dev, &trig, lq_sensor_trigger_##node_id); \
        } \
    } while (0)

/**
 * @brief Generate complete engine struct from devicetree
 * 
 * This macro creates a fully initialized lq_engine struct from DTS.
 * Place this in your application's main.c:
 * 
 *   struct lq_engine my_engine = LQ_ENGINE_DT_INIT(DT_NODELABEL(engine));
 */
#define LQ_ENGINE_DT_INIT(engine_node) \
    { \
        .num_signals = DT_PROP_OR(engine_node, max_signals, CONFIG_LQ_MAX_SIGNALS), \
        .num_merges = LQ_DT_NUM_COMPAT(lq_mid_merge), \
        .num_cyclic_outputs = LQ_DT_NUM_COMPAT(lq_cyclic_output), \
        .signals = {0}, \
        .merges = { LQ_FOREACH_MERGE(LQ_MERGE_CTX_INIT) }, \
        .cyclic_outputs = { LQ_FOREACH_CYCLIC_OUTPUT(LQ_CYCLIC_CTX_INIT) }, \
    }

#ifdef __cplusplus
}
#endif

#endif /* LQ_DEVICETREE_H_ */
