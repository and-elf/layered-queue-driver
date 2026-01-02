/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Unit tests for layered queue utilities
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/layered_queue_internal.h>

ZTEST_SUITE(lq_util_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(lq_util_tests, test_validate_range_basic)
{
    struct lq_range ranges[] = {
        { .min = 100, .max = 200, .status = LQ_OK },
        { .min = 50, .max = 250, .status = LQ_DEGRADED },
        { .min = 0, .max = 4095, .status = LQ_OUT_OF_RANGE },
    };

    /* Test normal range */
    zassert_equal(lq_validate_range(150, ranges, 3, LQ_ERROR), LQ_OK);

    /* Test degraded range */
    zassert_equal(lq_validate_range(75, ranges, 3, LQ_ERROR), LQ_DEGRADED);
    zassert_equal(lq_validate_range(225, ranges, 3, LQ_ERROR), LQ_DEGRADED);

    /* Test out of range */
    zassert_equal(lq_validate_range(10, ranges, 3, LQ_ERROR), LQ_OUT_OF_RANGE);
    zassert_equal(lq_validate_range(3000, ranges, 3, LQ_ERROR), LQ_OUT_OF_RANGE);
}

ZTEST(lq_util_tests, test_validate_range_first_match)
{
    /* First match should win */
    struct lq_range ranges[] = {
        { .min = 100, .max = 200, .status = LQ_OK },
        { .min = 150, .max = 250, .status = LQ_DEGRADED },
    };

    /* 150 is in both ranges, should get LQ_OK (first match) */
    zassert_equal(lq_validate_range(150, ranges, 2, LQ_ERROR), LQ_OK);
}

ZTEST(lq_util_tests, test_validate_value)
{
    struct lq_expected_value expected[] = {
        { .value = 0x55, .status = LQ_OK },
        { .value = 0x5A, .status = LQ_DEGRADED },
        { .value = 0xFF, .status = LQ_ERROR },
    };

    zassert_equal(lq_validate_value(0x55, expected, 3, LQ_OUT_OF_RANGE), LQ_OK);
    zassert_equal(lq_validate_value(0x5A, expected, 3, LQ_OUT_OF_RANGE), LQ_DEGRADED);
    zassert_equal(lq_validate_value(0xFF, expected, 3, LQ_OUT_OF_RANGE), LQ_ERROR);
    zassert_equal(lq_validate_value(0x00, expected, 3, LQ_OUT_OF_RANGE), LQ_OUT_OF_RANGE);
}

ZTEST(lq_util_tests, test_vote_median)
{
    int32_t values[] = { 100, 105, 200 };
    int32_t result;
    enum lq_status status;

    int ret = lq_vote(values, 3, LQ_VOTE_MEDIAN, 0, &result, &status);

    zassert_equal(ret, 0);
    zassert_equal(result, 105, "Median should be 105");
}

ZTEST(lq_util_tests, test_vote_average)
{
    int32_t values[] = { 100, 200, 300 };
    int32_t result;
    enum lq_status status;

    int ret = lq_vote(values, 3, LQ_VOTE_AVERAGE, 0, &result, &status);

    zassert_equal(ret, 0);
    zassert_equal(result, 200, "Average should be 200");
}

ZTEST(lq_util_tests, test_vote_min_max)
{
    int32_t values[] = { 300, 100, 200 };
    int32_t result;
    enum lq_status status;

    /* Test min */
    int ret = lq_vote(values, 3, LQ_VOTE_MIN, 0, &result, &status);
    zassert_equal(ret, 0);
    zassert_equal(result, 100);

    /* Test max */
    ret = lq_vote(values, 3, LQ_VOTE_MAX, 0, &result, &status);
    zassert_equal(ret, 0);
    zassert_equal(result, 300);
}

ZTEST(lq_util_tests, test_vote_tolerance)
{
    int32_t values_consistent[] = { 100, 102, 101 };
    int32_t values_inconsistent[] = { 100, 200, 101 };
    int32_t result;
    enum lq_status status;

    /* Consistent within tolerance=5 */
    int ret = lq_vote(values_consistent, 3, LQ_VOTE_MEDIAN, 5, &result, &status);
    zassert_equal(ret, 0);
    zassert_equal(status, LQ_OK);

    /* Inconsistent beyond tolerance=5 */
    ret = lq_vote(values_inconsistent, 3, LQ_VOTE_MEDIAN, 5, &result, &status);
    zassert_equal(ret, 0);
    zassert_equal(status, LQ_INCONSISTENT);
}

ZTEST(lq_util_tests, test_vote_invalid_params)
{
    int32_t values[] = { 100, 200 };
    int32_t result;
    enum lq_status status;

    /* NULL values */
    zassert_equal(lq_vote(NULL, 2, LQ_VOTE_MEDIAN, 0, &result, &status), -EINVAL);

    /* NULL result */
    zassert_equal(lq_vote(values, 2, LQ_VOTE_MEDIAN, 0, NULL, &status), -EINVAL);

    /* NULL status */
    zassert_equal(lq_vote(values, 2, LQ_VOTE_MEDIAN, 0, &result, NULL), -EINVAL);

    /* Zero values */
    zassert_equal(lq_vote(values, 0, LQ_VOTE_MEDIAN, 0, &result, &status), -EINVAL);
}
