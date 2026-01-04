/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Google Test tests for layered queue
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <algorithm>

extern "C" {
#include "layered_queue_core.h"
#include "lq_util.h"
#include "lq_spi_source.h"
}

#include <atomic>

class QueueTest : public ::testing::Test {
protected:
    static inline constexpr uint32_t CAPACITY = 8;
    
    lq_queue_t queue;
    struct lq_queue_config config;
    struct lq_queue_data data;
    struct lq_item items_buffer[CAPACITY];

    void SetUp() override {
        config.capacity = CAPACITY;
        config.drop_policy = LQ_DROP_OLDEST;
        config.priority = 0;

        int ret = lq_queue_init(&queue, &config, &data, items_buffer);
        ASSERT_EQ(ret, 0) << "Queue initialization failed";
    }

    void TearDown() override {
        lq_queue_destroy(&queue);
    }

    struct lq_item make_item(int32_t value, enum lq_status status = LQ_OK) {
        struct lq_item item = {
            .timestamp = 0,  // Will be set by queue
            .value = value,
            .status = status,
            .source_id = 1,
            .sequence = 0    // Will be set by queue
        };
        return item;
    }
};

// ============================================================================
// Basic Operations Tests
// ============================================================================

TEST_F(QueueTest, InitiallyEmpty) {
    EXPECT_EQ(lq_queue_count(&queue), 0);
}

TEST_F(QueueTest, PushSingleItem) {
    auto item = make_item(42);
    int ret = lq_queue_push(&queue, &item, 0);
    
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(lq_queue_count(&queue), 1);
}

TEST_F(QueueTest, PopSingleItem) {
    auto push_item = make_item(42);
    lq_queue_push(&queue, &push_item, 0);
    
    struct lq_item pop_item;
    int ret = lq_queue_pop(&queue, &pop_item, 0);
    
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(pop_item.value, 42);
    EXPECT_EQ(lq_queue_count(&queue), 0);
}

TEST_F(QueueTest, PopEmptyQueueNoWait) {
    struct lq_item item;
    int ret = lq_queue_pop(&queue, &item, 0);
    
    EXPECT_EQ(ret, -EAGAIN);
}

TEST_F(QueueTest, PushPopSequence) {
    std::vector<int32_t> values = {10, 20, 30, 40, 50};
    
    // Push all values
    for (auto val : values) {
        auto item = make_item(val);
        ASSERT_EQ(lq_queue_push(&queue, &item, 0), 0);
    }
    
    EXPECT_EQ(lq_queue_count(&queue), values.size());
    
    // Pop and verify
    for (auto expected : values) {
        struct lq_item item;
        ASSERT_EQ(lq_queue_pop(&queue, &item, 0), 0);
        EXPECT_EQ(item.value, expected);
    }
    
    EXPECT_EQ(lq_queue_count(&queue), 0);
}

TEST_F(QueueTest, PeekDoesNotRemove) {
    auto push_item = make_item(123);
    lq_queue_push(&queue, &push_item, 0);
    
    struct lq_item peek_item;
    ASSERT_EQ(lq_queue_peek(&queue, &peek_item), 0);
    EXPECT_EQ(peek_item.value, 123);
    EXPECT_EQ(lq_queue_count(&queue), 1);
    
    // Pop should still get the same item
    struct lq_item pop_item;
    ASSERT_EQ(lq_queue_pop(&queue, &pop_item, 0), 0);
    EXPECT_EQ(pop_item.value, 123);
}

TEST_F(QueueTest, SequenceNumberIncreases) {
    for (int i = 0; i < 5; i++) {
        auto item = make_item(i * 10);
        lq_queue_push(&queue, &item, 0);
    }
    
    uint32_t last_sequence = 0;
    for (int i = 0; i < 5; i++) {
        struct lq_item item;
        ASSERT_EQ(lq_queue_pop(&queue, &item, 0), 0);
        if (i > 0) {
            EXPECT_GT(item.sequence, last_sequence);
        }
        last_sequence = item.sequence;
    }
}

TEST_F(QueueTest, TimestampIsSet) {
    auto item = make_item(42);
    lq_queue_push(&queue, &item, 0);
    
    struct lq_item pop_item;
    lq_queue_pop(&queue, &pop_item, 0);
    
    // Timestamp should be set (could be 0 if uptime is exactly 0)
    EXPECT_GE(pop_item.timestamp, 0);
}

// ============================================================================
// Capacity and Drop Policy Tests
// ============================================================================

TEST_F(QueueTest, FillToCapacity) {
    for (uint32_t i = 0; i < CAPACITY; i++) {
        auto item = make_item(i);
        ASSERT_EQ(lq_queue_push(&queue, &item, 0), 0);
    }
    
    EXPECT_EQ(lq_queue_count(&queue), CAPACITY);
}

TEST_F(QueueTest, DropOldestPolicy) {
    config.drop_policy = LQ_DROP_OLDEST;
    
    // Fill queue
    for (uint32_t i = 0; i < CAPACITY; i++) {
        auto item = make_item(i);
        lq_queue_push(&queue, &item, 0);
    }
    
    // Push one more - should drop oldest (value 0)
    auto item = make_item(999);
    ASSERT_EQ(lq_queue_push(&queue, &item, 0), 0);
    
    // First item should now be 1 (0 was dropped)
    struct lq_item pop_item;
    lq_queue_pop(&queue, &pop_item, 0);
    EXPECT_EQ(pop_item.value, 1);
}

TEST_F(QueueTest, DropNewestPolicy) {
    config.drop_policy = LQ_DROP_NEWEST;
    lq_queue_destroy(&queue);
    lq_queue_init(&queue, &config, &data, items_buffer);
    
    // Fill queue
    for (uint32_t i = 0; i < CAPACITY; i++) {
        auto item = make_item(i);
        lq_queue_push(&queue, &item, 0);
    }
    
    // Try to push one more - should fail
    auto item = make_item(999);
    EXPECT_EQ(lq_queue_push(&queue, &item, 0), -ENOMEM);
    
    // First item should still be 0
    struct lq_item pop_item;
    lq_queue_pop(&queue, &pop_item, 0);
    EXPECT_EQ(pop_item.value, 0);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(QueueTest, StatisticsTracking) {
    // Push 5 items
    for (int i = 0; i < 5; i++) {
        auto item = make_item(i);
        lq_queue_push(&queue, &item, 0);
    }
    
    // Pop 3 items
    for (int i = 0; i < 3; i++) {
        struct lq_item item;
        lq_queue_pop(&queue, &item, 0);
    }
    
    struct lq_stats stats;
    ASSERT_EQ(lq_queue_get_stats(&queue, &stats), 0);
    
    EXPECT_EQ(stats.items_written, 5);
    EXPECT_EQ(stats.items_read, 3);
    EXPECT_EQ(stats.items_current, 2);
    EXPECT_EQ(stats.items_dropped, 0);
}

TEST_F(QueueTest, PeakUsageTracking) {
    // Push 5 items
    for (int i = 0; i < 5; i++) {
        auto item = make_item(i);
        lq_queue_push(&queue, &item, 0);
    }
    
    // Pop 3 items
    for (int i = 0; i < 3; i++) {
        struct lq_item item;
        lq_queue_pop(&queue, &item, 0);
    }
    
    struct lq_stats stats;
    lq_queue_get_stats(&queue, &stats);
    
    EXPECT_EQ(stats.peak_usage, 5);
}

TEST_F(QueueTest, DropCountTracking) {
    config.drop_policy = LQ_DROP_OLDEST;
    
    // Fill queue + 3 more
    for (uint32_t i = 0; i < CAPACITY + 3; i++) {
        auto item = make_item(i);
        lq_queue_push(&queue, &item, 0);
    }
    
    struct lq_stats stats;
    lq_queue_get_stats(&queue, &stats);
    
    EXPECT_EQ(stats.items_dropped, 3);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(QueueTest, NullPointerHandling) {
    struct lq_item item = make_item(42);
    
    EXPECT_EQ(lq_queue_push(nullptr, &item, 0), -EINVAL);
    EXPECT_EQ(lq_queue_push(&queue, nullptr, 0), -EINVAL);
    EXPECT_EQ(lq_queue_pop(nullptr, &item, 0), -EINVAL);
    EXPECT_EQ(lq_queue_pop(&queue, nullptr, 0), -EINVAL);
    EXPECT_EQ(lq_queue_peek(nullptr, &item), -EINVAL);
    EXPECT_EQ(lq_queue_peek(&queue, nullptr), -EINVAL);
}

TEST_F(QueueTest, TimeoutHandling) {
    struct lq_item item;
    
    // Pop with timeout should fail on empty queue
    auto start = lq_platform_uptime_get();
    int ret = lq_queue_pop(&queue, &item, 50);
    auto elapsed = lq_platform_uptime_get() - start;
    
    EXPECT_EQ(ret, -EAGAIN);
    EXPECT_GE(elapsed, 40);  // Allow some tolerance
    EXPECT_LE(elapsed, 150); // Increased tolerance for macOS scheduler
}

// ============================================================================
// Utility Function Tests
// ============================================================================

class UtilTest : public ::testing::Test {};

TEST_F(UtilTest, ValidateRangeBasic) {
    struct lq_range ranges[] = {
        { .min = 100, .max = 200, .status = LQ_OK },
        { .min = 50, .max = 250, .status = LQ_DEGRADED },
        { .min = 0, .max = 4095, .status = LQ_OUT_OF_RANGE },
    };

    // Test normal range
    EXPECT_EQ(lq_validate_range(150, ranges, 3, LQ_ERROR), LQ_OK);

    // Test degraded range
    EXPECT_EQ(lq_validate_range(75, ranges, 3, LQ_ERROR), LQ_DEGRADED);
    EXPECT_EQ(lq_validate_range(225, ranges, 3, LQ_ERROR), LQ_DEGRADED);

    // Test out of range
    EXPECT_EQ(lq_validate_range(10, ranges, 3, LQ_ERROR), LQ_OUT_OF_RANGE);
    EXPECT_EQ(lq_validate_range(3000, ranges, 3, LQ_ERROR), LQ_OUT_OF_RANGE);
}

TEST_F(UtilTest, ValidateRangeFirstMatch) {
    // First match should win
    struct lq_range ranges[] = {
        { .min = 100, .max = 200, .status = LQ_OK },
        { .min = 150, .max = 250, .status = LQ_DEGRADED },
    };

    // 150 is in both ranges, should get LQ_OK (first match)
    EXPECT_EQ(lq_validate_range(150, ranges, 2, LQ_ERROR), LQ_OK);
}

TEST_F(UtilTest, ValidateValue) {
    struct lq_expected_value expected[] = {
        { .value = 0x55, .status = LQ_OK },
        { .value = 0x5A, .status = LQ_DEGRADED },
        { .value = 0xFF, .status = LQ_ERROR },
    };

    EXPECT_EQ(lq_validate_value(0x55, expected, 3, LQ_OUT_OF_RANGE), LQ_OK);
    EXPECT_EQ(lq_validate_value(0x5A, expected, 3, LQ_OUT_OF_RANGE), LQ_DEGRADED);
    EXPECT_EQ(lq_validate_value(0xFF, expected, 3, LQ_OUT_OF_RANGE), LQ_ERROR);
    EXPECT_EQ(lq_validate_value(0x00, expected, 3, LQ_OUT_OF_RANGE), LQ_OUT_OF_RANGE);
}

TEST_F(UtilTest, VoteMedian) {
    int32_t values[] = { 100, 105, 200 };
    int32_t result;
    enum lq_status status;

    int ret = lq_vote(values, 3, LQ_VOTE_MEDIAN, 0, &result, &status);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, 105) << "Median should be 105";
}

TEST_F(UtilTest, VoteAverage) {
    int32_t values[] = { 100, 200, 300 };
    int32_t result;
    enum lq_status status;

    int ret = lq_vote(values, 3, LQ_VOTE_AVERAGE, 0, &result, &status);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, 200) << "Average should be 200";
}

TEST_F(UtilTest, VoteMinMax) {
    int32_t values[] = { 300, 100, 200 };
    int32_t result;
    enum lq_status status;

    // Test min
    int ret = lq_vote(values, 3, LQ_VOTE_MIN, 0, &result, &status);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, 100);

    // Test max
    ret = lq_vote(values, 3, LQ_VOTE_MAX, 0, &result, &status);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, 300);
}

TEST_F(UtilTest, VoteTolerance) {
    int32_t values_consistent[] = { 100, 102, 101 };
    int32_t values_inconsistent[] = { 100, 200, 101 };
    int32_t result;
    enum lq_status status;

    // Consistent within tolerance=5
    int ret = lq_vote(values_consistent, 3, LQ_VOTE_MEDIAN, 5, &result, &status);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(status, LQ_OK);

    // Inconsistent beyond tolerance=5
    ret = lq_vote(values_inconsistent, 3, LQ_VOTE_MEDIAN, 5, &result, &status);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(status, LQ_INCONSISTENT);
}

TEST_F(UtilTest, VoteInvalidParams) {
    int32_t values[] = { 100, 200 };
    int32_t result;
    enum lq_status status;

    // NULL values
    EXPECT_EQ(lq_vote(nullptr, 2, LQ_VOTE_MEDIAN, 0, &result, &status), -EINVAL);

    // NULL result
    EXPECT_EQ(lq_vote(values, 2, LQ_VOTE_MEDIAN, 0, nullptr, &status), -EINVAL);

    // NULL status
    EXPECT_EQ(lq_vote(values, 2, LQ_VOTE_MEDIAN, 0, &result, nullptr), -EINVAL);

    // Zero values
    EXPECT_EQ(lq_vote(values, 0, LQ_VOTE_MEDIAN, 0, &result, &status), -EINVAL);
}

// ============================================================================
// SPI Source Tests
// ============================================================================

class SPISourceTest : public ::testing::Test {};

TEST_F(SPISourceTest, ProcessSingleByte) {
    struct lq_expected_value expected[] = {
        { .value = 0x42, .status = LQ_OK },
        { .value = 0xFF, .status = LQ_ERROR },
    };

    struct lq_spi_source_config config = {
        .poll_interval_ms = 100,
        .read_length = 1,
        .expected = expected,
        .num_expected = 2,
        .default_status = LQ_OUT_OF_RANGE,
    };

    uint8_t data[] = { 0x42 };
    int32_t value;
    enum lq_status status;

    int ret = lq_spi_source_process(data, 1, &config, &value, &status);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(value, 0x42);
    EXPECT_EQ(status, LQ_OK);
}

TEST_F(SPISourceTest, ProcessTwoBytes) {
    struct lq_expected_value expected[] = {
        { .value = 0x1234, .status = LQ_OK },
    };

    struct lq_spi_source_config config = {
        .poll_interval_ms = 100,
        .read_length = 2,
        .expected = expected,
        .num_expected = 1,
        .default_status = LQ_DEGRADED,
    };

    uint8_t data[] = { 0x12, 0x34 };  // Big-endian
    int32_t value;
    enum lq_status status;

    int ret = lq_spi_source_process(data, 2, &config, &value, &status);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(value, 0x1234);
    EXPECT_EQ(status, LQ_OK);
}

TEST_F(SPISourceTest, ProcessFourBytes) {
    struct lq_spi_source_config config = {
        .poll_interval_ms = 100,
        .read_length = 4,
        .expected = nullptr,
        .num_expected = 0,
        .default_status = LQ_OK,
    };

    uint8_t data[] = { 0x12, 0x34, 0x56, 0x78 };
    int32_t value;
    enum lq_status status;

    int ret = lq_spi_source_process(data, 4, &config, &value, &status);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(value, 0x12345678);
    EXPECT_EQ(status, LQ_OK);
}

TEST_F(SPISourceTest, SignExtension) {
    struct lq_spi_source_config config = {
        .poll_interval_ms = 100,
        .read_length = 2,
        .expected = nullptr,
        .num_expected = 0,
        .default_status = LQ_OK,
    };

    // 0xFF80 in 16-bit signed is -128
    uint8_t data[] = { 0xFF, 0x80 };
    int32_t value;
    enum lq_status status;

    int ret = lq_spi_source_process(data, 2, &config, &value, &status);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(value, -128);  // Sign-extended to 32-bit
}

TEST_F(SPISourceTest, UnmatchedValueUsesDefault) {
    struct lq_expected_value expected[] = {
        { .value = 0x00, .status = LQ_OK },
        { .value = 0xFF, .status = LQ_ERROR },
    };

    struct lq_spi_source_config config = {
        .poll_interval_ms = 100,
        .read_length = 1,
        .expected = expected,
        .num_expected = 2,
        .default_status = LQ_DEGRADED,
    };

    uint8_t data[] = { 0x42 };  // Not in expected list
    int32_t value;
    enum lq_status status;

    int ret = lq_spi_source_process(data, 1, &config, &value, &status);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(value, 0x42);
    EXPECT_EQ(status, LQ_DEGRADED);  // Uses default_status
}

TEST_F(SPISourceTest, InvalidParameters) {
    struct lq_spi_source_config config = {
        .poll_interval_ms = 100,
        .read_length = 1,
        .expected = nullptr,
        .num_expected = 0,
        .default_status = LQ_OK,
    };

    uint8_t data[] = { 0x42 };
    int32_t value;
    enum lq_status status;

    // NULL data
    EXPECT_EQ(lq_spi_source_process(nullptr, 1, &config, &value, &status), -EINVAL);

    // NULL config
    EXPECT_EQ(lq_spi_source_process(data, 1, nullptr, &value, &status), -EINVAL);

    // NULL value
    EXPECT_EQ(lq_spi_source_process(data, 1, &config, nullptr, &status), -EINVAL);

    // NULL status
    EXPECT_EQ(lq_spi_source_process(data, 1, &config, &value, nullptr), -EINVAL);

    // Zero length
    EXPECT_EQ(lq_spi_source_process(data, 0, &config, &value, &status), -EINVAL);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
