/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform-independent SPI source logic
 */

#ifndef LQ_SPI_SOURCE_H
#define LQ_SPI_SOURCE_H

#include <stdint.h>
#include "layered_queue_core.h"
#include "lq_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SPI source configuration (platform-independent)
 */
struct lq_spi_source_config {
    uint32_t poll_interval_ms;          /**< Polling interval */
    uint8_t read_length;                /**< Bytes to read per transaction */
    const struct lq_expected_value *expected; /**< Expected value mappings */
    uint8_t num_expected;               /**< Number of expected values */
    enum lq_status default_status;      /**< Status when no match */
};

/**
 * @brief Process raw SPI bytes into a value with status
 * 
 * Converts multi-byte SPI read into a single value and validates
 * against expected values.
 * 
 * @param data Raw bytes from SPI (big-endian)
 * @param length Number of bytes
 * @param config Source configuration
 * @param value Output: processed value
 * @param status Output: validation status
 * @return 0 on success, negative errno on error
 */
int lq_spi_source_process(const uint8_t *data,
                          uint8_t length,
                          const struct lq_spi_source_config *config,
                          int32_t *value,
                          enum lq_status *status);

#ifdef __cplusplus
}
#endif

#endif /* LQ_SPI_SOURCE_H */
