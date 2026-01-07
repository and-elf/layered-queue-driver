/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Utility functions for range validation and voting (platform-independent)
 */

#include "lq_util.h"
#include <stdlib.h>
#include <string.h>

enum lq_status lq_validate_range(int32_t value,
                                   const struct lq_range *ranges,
                                   uint8_t num_ranges,
                                   enum lq_status default_status)
{
    if (!ranges || num_ranges == 0) {
        return default_status;
    }

    /* Check ranges in order (first match wins) */
    for (uint8_t i = 0; i < num_ranges; i++) {
        if (value >= ranges[i].min && value <= ranges[i].max) {
            return ranges[i].status;
        }
    }

    return default_status;
}

enum lq_status lq_validate_value(int32_t value,
                                   const struct lq_expected_value *expected,
                                   uint8_t num_expected,
                                   enum lq_status default_status)
{
    if (!expected || num_expected == 0) {
        return default_status;
    }

    /* Look for exact match */
    for (uint8_t i = 0; i < num_expected; i++) {
        if (value == expected[i].value) {
            return expected[i].status;
        }
    }

    return default_status;
}

/* Comparison function for qsort */
static int compare_int32(const void *a, const void *b)
{
    int32_t val_a = *(const int32_t *)a;
    int32_t val_b = *(const int32_t *)b;
    return (val_a > val_b) - (val_a < val_b);
}

int lq_vote(const int32_t *values,
            uint8_t num_values,
            enum lq_vote_method method,
            int32_t tolerance,
            int32_t *result,
            enum lq_status *status)
{
    if (!values || num_values == 0 || !result || !status) {
        return -EINVAL;
    }

    *status = LQ_OK;

    switch (method) {
    case LQ_VOTE_MEDIAN: {
        /* Create sorted copy */
        int32_t sorted[num_values];
        memcpy(sorted, values, num_values * sizeof(int32_t));
        qsort(sorted, num_values, sizeof(int32_t), compare_int32);
        *result = sorted[num_values / 2];
        break;
    }

    case LQ_VOTE_AVERAGE: {
        int64_t sum = 0;
        for (uint8_t i = 0; i < num_values; i++) {
            sum += values[i];
        }
        *result = (int32_t)(sum / num_values);
        break;
    }

    case LQ_VOTE_MIN:
        *result = values[0];
        for (uint8_t i = 1; i < num_values; i++) {
            if (values[i] < *result) {
                *result = values[i];
            }
        }
        break;

    case LQ_VOTE_MAX:
        *result = values[0];
        for (uint8_t i = 1; i < num_values; i++) {
            if (values[i] > *result) {
                *result = values[i];
            }
        }
        break;

    default:
        *result = values[0];
        break;
    }

    /* Check tolerance for all methods */
    if (tolerance > 0) {
        int32_t max_deviation = 0;
        for (uint8_t i = 0; i < num_values; i++) {
            int32_t deviation = abs(values[i] - *result);
            if (deviation > max_deviation) {
                max_deviation = deviation;
            }
        }

        if (max_deviation > tolerance && *status == LQ_OK) {
            *status = LQ_INCONSISTENT;
        }
    }

    return 0;
}
