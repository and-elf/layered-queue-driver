/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform-independent SPI source logic
 */

#include "lq_spi_source.h"
#include <string.h>

int lq_spi_source_process(const uint8_t *data,
                          uint8_t length,
                          const struct lq_spi_source_config *config,
                          int32_t *value,
                          enum lq_status *status)
{
    if (!data || !config || !value || !status || length == 0) {
        return -EINVAL;
    }

    /* Convert bytes to value (big-endian) */
    int32_t result = 0;
    for (uint8_t i = 0; i < length && i < 4; i++) {
        result = (result << 8) | data[i];
    }

    /* Sign-extend if needed for values < 4 bytes */
    if (length < 4) {
        uint8_t sign_bit = 8 * length - 1;
        if (result & (1 << sign_bit)) {
            /* Negative - extend sign bits */
            int32_t mask = ~((1 << (sign_bit + 1)) - 1);
            result |= mask;
        }
    }

    *value = result;

    /* Validate against expected values */
    *status = lq_validate_value(result,
                                config->expected,
                                config->num_expected,
                                config->default_status);

    return 0;
}
