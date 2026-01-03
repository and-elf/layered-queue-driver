/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Utility functions for range validation and voting (platform-independent)
 */

#ifndef LQ_UTIL_H
#define LQ_UTIL_H

#include "lq_common.h"
#include <stdint.h>
#include "layered_queue_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Range validation
 * ============================================================================ */

/**
 * @brief Range specification for value validation
 */
struct lq_range {
    int32_t min;             /**< Minimum value (inclusive) */
    int32_t max;             /**< Maximum value (inclusive) */
    enum lq_status status;   /**< Status to return if in range */
};

/**
 * @brief Expected value specification
 */
struct lq_expected_value {
    int32_t value;           /**< Expected value */
    enum lq_status status;   /**< Status to return if matched */
};

/**
 * @brief Validate value against ranges
 * 
 * @param value Value to validate
 * @param ranges Array of range specifications (checked in order)
 * @param num_ranges Number of ranges
 * @param default_status Status to return if no range matches
 * @return Status based on first matching range, or default_status
 */
enum lq_status lq_validate_range(int32_t value,
                                   const struct lq_range *ranges,
                                   uint8_t num_ranges,
                                   enum lq_status default_status);

/**
 * @brief Validate value against expected values
 * 
 * @param value Value to validate
 * @param expected Array of expected value specifications
 * @param num_expected Number of expected values
 * @param default_status Status to return if no match
 * @return Status based on matching expected value, or default_status
 */
enum lq_status lq_validate_value(int32_t value,
                                   const struct lq_expected_value *expected,
                                   uint8_t num_expected,
                                   enum lq_status default_status);

/* ============================================================================
 * Voting/Merging
 * ============================================================================ */

/**
 * @brief Vote on multiple values using specified method
 * 
 * @param values Array of input values
 * @param num_values Number of values
 * @param method Voting method to use
 * @param tolerance Maximum allowed deviation (0 = no check)
 * @param result Output: voted result
 * @param status Output: LQ_OK if consistent, LQ_INCONSISTENT if deviation > tolerance
 * @return 0 on success, -EINVAL on invalid parameters
 */
int lq_vote(const int32_t *values,
            uint8_t num_values,
            enum lq_vote_method method,
            int32_t tolerance,
            int32_t *result,
            enum lq_status *status);

#ifdef __cplusplus
}
#endif

#endif /* LQ_UTIL_H */
