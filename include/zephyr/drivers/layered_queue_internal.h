/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_LAYERED_QUEUE_INTERNAL_H_
#define ZEPHYR_DRIVERS_LAYERED_QUEUE_INTERNAL_H_

#include <zephyr/devicetree.h>
#include <zephyr/drivers/layered_queue.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal utilities for layered queue drivers
 * @defgroup lq_internal Layered Queue Internal API
 * @{
 */

/**
 * @brief Queue runtime data structure
 */
struct lq_queue_data {
    /** Ring buffer for queue items */
    struct lq_item *items;
    /** Read index */
    uint32_t read_idx;
    /** Write index */
    uint32_t write_idx;
    /** Current number of items */
    atomic_t count;
    /** Statistics */
    struct lq_stats stats;
    /** Semaphore for blocking operations */
    struct k_sem sem_read;
    struct k_sem sem_write;
    /** Mutex for thread safety */
    struct k_mutex mutex;
    /** Callback and user data */
    lq_callback_t callback;
    void *callback_user_data;
    /** Sequence counter */
    uint32_t sequence;
};

/**
 * @brief Queue configuration structure
 */
struct lq_queue_config {
    /** Queue capacity */
    uint32_t capacity;
    /** Drop policy */
    enum lq_drop_policy drop_policy;
    /** Priority */
    int priority;
};

/**
 * @brief ADC source configuration
 */
struct lq_adc_source_config {
    /** ADC device */
    const struct device *adc_dev;
    /** ADC channel */
    uint8_t channel;
    /** Output queue */
    const struct device *output_queue;
    /** Poll interval in ms */
    uint32_t poll_interval_ms;
    /** Number of samples to average */
    uint8_t averaging;
    /** Expected ranges */
    const struct lq_range *ranges;
    /** Number of ranges */
    uint8_t num_ranges;
};

/**
 * @brief ADC source runtime data
 */
struct lq_adc_source_data {
    /** Work queue item for polling */
    struct k_work_delayable poll_work;
    /** Sequence counter */
    uint32_t sequence;
    /** Sample buffer for averaging */
    int32_t *samples;
    /** Current sample index */
    uint8_t sample_idx;
};

/**
 * @brief SPI source configuration
 */
struct lq_spi_source_config {
    /** SPI device */
    const struct device *spi_dev;
    /** SPI register/CS */
    uint32_t reg;
    /** Output queue */
    const struct device *output_queue;
    /** Poll interval in ms */
    uint32_t poll_interval_ms;
    /** Read length in bytes */
    uint8_t read_length;
    /** Expected values */
    const struct lq_expected_value *expected_values;
    /** Number of expected values */
    uint8_t num_expected_values;
};

/**
 * @brief SPI source runtime data
 */
struct lq_spi_source_data {
    /** Work queue item for polling */
    struct k_work_delayable poll_work;
    /** Sequence counter */
    uint32_t sequence;
};

/**
 * @brief Merge/voter configuration
 */
struct lq_merge_config {
    /** Input queues */
    const struct device **input_queues;
    /** Number of input queues */
    uint8_t num_inputs;
    /** Output queue */
    const struct device *output_queue;
    /** Voting method */
    enum lq_voting_method voting_method;
    /** Tolerance for voting */
    int32_t tolerance;
    /** Status if inputs violate tolerance */
    enum lq_status status_if_violation;
    /** Timeout in ms */
    uint32_t timeout_ms;
    /** Expected range (optional) */
    const struct lq_range *expected_range;
};

/**
 * @brief Merge/voter runtime data
 */
struct lq_merge_data {
    /** Work queue item for merging */
    struct k_work_delayable merge_work;
    /** Sequence counter */
    uint32_t sequence;
    /** Last values from each input */
    int32_t *last_values;
    /** Timestamps of last values */
    uint32_t *last_timestamps;
};

/**
 * @brief Dual-inverted input configuration
 */
struct lq_dual_inverted_config {
    /** Normal GPIO spec */
    struct gpio_dt_spec gpio_normal;
    /** Inverted GPIO spec */
    struct gpio_dt_spec gpio_inverted;
    /** Output queue */
    const struct device *output_queue;
    /** Poll interval in ms */
    uint32_t poll_interval_ms;
    /** Debounce time in ms */
    uint32_t debounce_ms;
    /** Error on both high */
    bool error_on_both_high;
    /** Error on both low */
    bool error_on_both_low;
    /** Error status code */
    enum lq_status error_status;
    /** OK status code */
    enum lq_status ok_status;
};

/**
 * @brief Dual-inverted input runtime data
 */
struct lq_dual_inverted_data {
    /** Work queue item for polling */
    struct k_work_delayable poll_work;
    /** Sequence counter */
    uint32_t sequence;
    /** Last stable state */
    bool last_state;
    /** Debounce counter */
    uint32_t debounce_count;
};

/**
 * @brief Validate a value against expected ranges
 *
 * @param value Value to validate
 * @param ranges Array of ranges
 * @param num_ranges Number of ranges
 * @param default_status Default status if no range matches
 * @return Status code
 */
enum lq_status lq_validate_range(int32_t value,
                                   const struct lq_range *ranges,
                                   uint8_t num_ranges,
                                   enum lq_status default_status);

/**
 * @brief Validate a value against expected discrete values
 *
 * @param value Value to validate
 * @param expected Array of expected values
 * @param num_expected Number of expected values
 * @param default_status Default status if no match
 * @return Status code
 */
enum lq_status lq_validate_value(int32_t value,
                                   const struct lq_expected_value *expected,
                                   uint8_t num_expected,
                                   enum lq_status default_status);

/**
 * @brief Perform voting on multiple input values
 *
 * @param values Array of input values
 * @param num_values Number of values
 * @param method Voting method
 * @param tolerance Tolerance for consistency check
 * @param result Pointer to store result
 * @param status Pointer to store status
 * @return 0 on success, negative errno on failure
 */
int lq_vote(const int32_t *values,
            uint8_t num_values,
            enum lq_voting_method method,
            int32_t tolerance,
            int32_t *result,
            enum lq_status *status);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_LAYERED_QUEUE_INTERNAL_H_ */
