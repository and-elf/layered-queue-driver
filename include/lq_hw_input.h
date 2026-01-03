/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hardware input abstraction layer
 * 
 * This layer provides a thin, RTOS-aware interface for collecting
 * raw hardware samples from ISRs and polling functions into a
 * common ringbuffer for processing by the engine.
 */

#ifndef LQ_HW_INPUT_H_
#define LQ_HW_INPUT_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware source identifiers
 * 
 * Each hardware input (ADC channel, SPI device, etc.) gets a unique ID
 * configured at build time from device tree.
 */
enum lq_hw_source {
    LQ_HW_ADC0 = 0,
    LQ_HW_ADC1,
    LQ_HW_ADC2,
    LQ_HW_ADC3,
    LQ_HW_SPI0,
    LQ_HW_SPI1,
    LQ_HW_GPIO0,
    LQ_HW_GPIO1,
    /* Add more as needed */
    LQ_HW_SOURCE_COUNT
};

/**
 * @brief Raw hardware sample
 * 
 * Captured from ISR or polling context and placed in ringbuffer.
 * Minimal processing - just capture the raw value and timestamp.
 */
struct lq_hw_sample {
    enum lq_hw_source src;     /**< Hardware source ID */
    uint32_t value;             /**< Raw hardware value */
    uint64_t timestamp;         /**< Capture timestamp (microseconds) */
};

/**
 * @brief Push a raw hardware sample into the input ringbuffer
 * 
 * This function is ISR-safe and can be called from interrupt context.
 * It captures the sample and timestamp for later processing by the engine.
 * 
 * Example usage from ADC ISR:
 * @code
 * void adc_isr_callback(uint16_t sample)
 * {
 *     lq_hw_push(LQ_HW_ADC0, sample);
 * }
 * @endcode
 * 
 * @param src Hardware source identifier
 * @param value Raw hardware value
 */
void lq_hw_push(enum lq_hw_source src, uint32_t value);

/**
 * @brief Initialize the hardware input ringbuffer
 * 
 * Must be called before any calls to lq_hw_push().
 * 
 * @param size Ringbuffer size in number of samples
 * @return 0 on success, negative errno on failure
 */
int lq_hw_input_init(size_t size);

/**
 * @brief Get the next hardware sample from the ringbuffer
 * 
 * Called by the engine to retrieve samples for processing.
 * This is the bridge between the RTOS layer and pure processing.
 * 
 * @param sample Output buffer for sample
 * @return 0 on success, -EAGAIN if empty, negative errno on error
 */
int lq_hw_pop(struct lq_hw_sample *sample);

/**
 * @brief Get number of samples currently in ringbuffer
 * 
 * @return Number of pending samples
 */
size_t lq_hw_pending(void);

#ifdef __cplusplus
}
#endif

#endif /* LQ_HW_INPUT_H_ */
