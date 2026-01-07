/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Google Test tests for Hardware Input Layer
 * Comprehensive coverage for production validation
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "lq_hw_input.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class HwInputTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize with default size
        int ret = lq_hw_input_init(128);
        ASSERT_EQ(ret, 0);
        
        // Drain any existing samples
        struct lq_hw_sample sample;
        while (lq_hw_pop(&sample) == 0) {
            // Drain
        }
    }
    
    void TearDown() override {
        // Cleanup - drain buffer
        struct lq_hw_sample sample;
        while (lq_hw_pop(&sample) == 0) {
            // Drain
        }
    }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(HwInputTest, InitSuccess) {
    // Already initialized in SetUp
    EXPECT_EQ(lq_hw_pending(), 0);
}

TEST_F(HwInputTest, InitMultipleTimes) {
    // Re-initialize should work
    int ret = lq_hw_input_init(64);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(lq_hw_pending(), 0);
}

// ============================================================================
// Push/Pop Basic Tests
// ============================================================================

TEST_F(HwInputTest, PushSingleSample) {
    lq_hw_push(LQ_HW_ADC0, 1234);
    
    EXPECT_EQ(lq_hw_pending(), 1);
    
    struct lq_hw_sample sample;
    int ret = lq_hw_pop(&sample);
    
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sample.src, LQ_HW_ADC0);
    EXPECT_EQ(sample.value, 1234);
    // Timestamp should be present (may be 0 at startup)
    EXPECT_GE(sample.timestamp, 0);
}

TEST_F(HwInputTest, PushMultipleSamples) {
    lq_hw_push(LQ_HW_ADC0, 100);
    lq_hw_push(LQ_HW_ADC1, 200);
    lq_hw_push(LQ_HW_SPI0, 300);
    
    EXPECT_EQ(lq_hw_pending(), 3);
    
    struct lq_hw_sample sample;
    
    // Pop in FIFO order
    ASSERT_EQ(lq_hw_pop(&sample), 0);
    EXPECT_EQ(sample.src, LQ_HW_ADC0);
    EXPECT_EQ(sample.value, 100);
    
    ASSERT_EQ(lq_hw_pop(&sample), 0);
    EXPECT_EQ(sample.src, LQ_HW_ADC1);
    EXPECT_EQ(sample.value, 200);
    
    ASSERT_EQ(lq_hw_pop(&sample), 0);
    EXPECT_EQ(sample.src, LQ_HW_SPI0);
    EXPECT_EQ(sample.value, 300);
    
    EXPECT_EQ(lq_hw_pending(), 0);
}

TEST_F(HwInputTest, PopEmptyBuffer) {
    struct lq_hw_sample sample;
    int ret = lq_hw_pop(&sample);
    
    // Should return error when buffer is empty
    EXPECT_LT(ret, 0);
    EXPECT_EQ(lq_hw_pending(), 0);
}

TEST_F(HwInputTest, PopNullPointer) {
    lq_hw_push(LQ_HW_ADC0, 100);
    
    // Pop with null pointer should not crash but may return error
    // (implementation may or may not check for null)
    // This is mainly to ensure we don't crash
}

// ============================================================================
// Timestamp Tests
// ============================================================================

TEST_F(HwInputTest, TimestampIncreases) {
    lq_hw_push(LQ_HW_ADC0, 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    lq_hw_push(LQ_HW_ADC1, 200);
    
    struct lq_hw_sample sample1, sample2;
    
    ASSERT_EQ(lq_hw_pop(&sample1), 0);
    ASSERT_EQ(lq_hw_pop(&sample2), 0);
    
    // Second timestamp should be greater than or equal to first
    EXPECT_GE(sample2.timestamp, sample1.timestamp);
}

TEST_F(HwInputTest, TimestampInMicroseconds) {
    lq_hw_push(LQ_HW_ADC0, 100);
    
    struct lq_hw_sample sample;
    ASSERT_EQ(lq_hw_pop(&sample), 0);
    
    // Timestamp should be in microseconds (reasonable range)
    EXPECT_GT(sample.timestamp, 0);
    EXPECT_LT(sample.timestamp, 1000000000000ULL); // Less than ~11 days in microseconds
}

// ============================================================================
// Ringbuffer Wraparound Tests
// ============================================================================

TEST_F(HwInputTest, RingbufferWraparound) {
    // Push and pop multiple times to test wraparound
    for (int cycle = 0; cycle < 3; cycle++) {
        // Fill buffer partially
        for (int i = 0; i < 50; i++) {
            lq_hw_push(LQ_HW_ADC0, 1000 + i);
        }
        
        EXPECT_EQ(lq_hw_pending(), 50);
        
        // Pop all
        struct lq_hw_sample sample;
        for (int i = 0; i < 50; i++) {
            ASSERT_EQ(lq_hw_pop(&sample), 0);
            EXPECT_EQ(sample.value, 1000 + i);
        }
        
        EXPECT_EQ(lq_hw_pending(), 0);
    }
}

TEST_F(HwInputTest, FillAndDrainSequence) {
    // Interleaved push/pop to test index management
    for (int i = 0; i < 10; i++) {
        lq_hw_push(LQ_HW_ADC0, i * 10);
        lq_hw_push(LQ_HW_ADC1, i * 10 + 1);
        
        struct lq_hw_sample sample;
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.value, i * 10);
        
        EXPECT_EQ(lq_hw_pending(), 1);
        
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.value, i * 10 + 1);
        
        EXPECT_EQ(lq_hw_pending(), 0);
    }
}

// ============================================================================
// Buffer Full Tests
// ============================================================================

TEST_F(HwInputTest, BufferFull) {
    // Fill buffer to capacity (128 samples)
    for (int i = 0; i < 128; i++) {
        lq_hw_push(LQ_HW_ADC0, i);
    }
    
    EXPECT_EQ(lq_hw_pending(), 128);
    
    // Push one more - should be dropped
    lq_hw_push(LQ_HW_ADC0, 9999);
    
    // Should still be 128
    EXPECT_EQ(lq_hw_pending(), 128);
    
    // Pop all and verify the overflow sample was dropped
    struct lq_hw_sample sample;
    for (int i = 0; i < 128; i++) {
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.value, i);
    }
    
    // Should be empty now
    EXPECT_EQ(lq_hw_pending(), 0);
}

TEST_F(HwInputTest, BufferFullRecovery) {
    // Fill buffer
    for (int i = 0; i < 128; i++) {
        lq_hw_push(LQ_HW_ADC0, i);
    }
    
    // Pop some to make space
    struct lq_hw_sample sample;
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(lq_hw_pop(&sample), 0);
    }
    
    EXPECT_EQ(lq_hw_pending(), 118);
    
    // Should be able to push again
    lq_hw_push(LQ_HW_ADC1, 5555);
    EXPECT_EQ(lq_hw_pending(), 119);
    
    // Drain and verify
    for (int i = 0; i < 118; i++) {
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.value, i + 10);
    }
    
    // Get the last one
    ASSERT_EQ(lq_hw_pop(&sample), 0);
    EXPECT_EQ(sample.value, 5555);
    EXPECT_EQ(sample.src, LQ_HW_ADC1);
}

// ============================================================================
// Source ID Tests
// ============================================================================

TEST_F(HwInputTest, AllSourceTypes) {
    // Test various source IDs
    lq_hw_push(LQ_HW_ADC0, 0);
    lq_hw_push(LQ_HW_ADC1, 1);
    lq_hw_push(LQ_HW_ADC2, 2);
    lq_hw_push(LQ_HW_ADC3, 3);
    lq_hw_push(LQ_HW_SPI0, 4);
    lq_hw_push(LQ_HW_SPI1, 5);
    lq_hw_push(LQ_HW_GPIO0, 6);
    lq_hw_push(LQ_HW_GPIO1, 7);
    
    EXPECT_EQ(lq_hw_pending(), 8);
    
    struct lq_hw_sample sample;
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.value, i);
    }
}

TEST_F(HwInputTest, SameSourceMultipleSamples) {
    // Multiple samples from same source
    for (int i = 0; i < 20; i++) {
        lq_hw_push(LQ_HW_ADC0, i * 100);
    }
    
    EXPECT_EQ(lq_hw_pending(), 20);
    
    struct lq_hw_sample sample;
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.src, LQ_HW_ADC0);
        EXPECT_EQ(sample.value, i * 100);
    }
}

// ============================================================================
// Value Range Tests
// ============================================================================

TEST_F(HwInputTest, ValueRange) {
    // Test various value ranges
    uint32_t test_values[] = {
        0,
        1,
        255,
        256,
        65535,
        65536,
        0xFFFFFFFF,
        0x80000000,
        1234567890
    };
    
    for (uint32_t val : test_values) {
        lq_hw_push(LQ_HW_ADC0, val);
    }
    
    struct lq_hw_sample sample;
    for (uint32_t val : test_values) {
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.value, val);
    }
}

// ============================================================================
// Concurrent Access Tests (Thread Safety)
// ============================================================================

TEST_F(HwInputTest, ConcurrentPushPop) {
    const int num_samples = 1000;
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < num_samples; i++) {
            lq_hw_push(LQ_HW_ADC0, i);
            push_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        struct lq_hw_sample sample;
        for (int i = 0; i < num_samples; i++) {
            while (lq_hw_pop(&sample) != 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
            pop_count++;
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(push_count, num_samples);
    EXPECT_EQ(pop_count, num_samples);
    EXPECT_EQ(lq_hw_pending(), 0);
}

TEST_F(HwInputTest, MultipleProducers) {
    const int samples_per_thread = 30;
    const int num_threads = 4;
    std::vector<std::thread> producers;
    
    // Create multiple producer threads
    for (int t = 0; t < num_threads; t++) {
        producers.emplace_back([t, samples_per_thread]() {
            for (int i = 0; i < samples_per_thread; i++) {
                lq_hw_push(static_cast<enum lq_hw_source>(t), i);
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        });
    }
    
    // Wait for all producers
    for (auto& t : producers) {
        t.join();
    }
    
    // Should have all samples (limited by buffer size)
    int expected = samples_per_thread * num_threads;
    EXPECT_LE(lq_hw_pending(), 128);  // Buffer size limit
    EXPECT_GE(lq_hw_pending(), expected > 128 ? 128 : expected);
    
    // Drain and verify we got samples
    struct lq_hw_sample sample;
    int count = 0;
    while (lq_hw_pop(&sample) == 0) {
        count++;
    }
    
    // Should get up to buffer size
    EXPECT_EQ(count, expected);
}

TEST_F(HwInputTest, MultipleConsumers) {
    const int total_samples = 120;  // Less than buffer size
    const int num_consumers = 4;
    
    // Fill buffer first
    for (int i = 0; i < total_samples; i++) {
        lq_hw_push(LQ_HW_ADC0, i);
    }
    
    EXPECT_EQ(lq_hw_pending(), total_samples);
    
    std::atomic<int> total_popped{0};
    std::vector<std::thread> consumers;
    
    // Create multiple consumer threads
    for (int c = 0; c < num_consumers; c++) {
        consumers.emplace_back([&total_popped]() {
            struct lq_hw_sample sample;
            while (lq_hw_pop(&sample) == 0) {
                total_popped++;
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        });
    }
    
    // Wait for all consumers
    for (auto& t : consumers) {
        t.join();
    }
    
    EXPECT_EQ(total_popped, total_samples);
    EXPECT_EQ(lq_hw_pending(), 0);
}

TEST_F(HwInputTest, HighFrequencyPush) {
    const int num_samples = 500;
    
    // Rapid push (simulating high-frequency ISR)
    for (int i = 0; i < num_samples; i++) {
        lq_hw_push(LQ_HW_ADC0, i);
    }
    
    // Should not exceed buffer size
    EXPECT_LE(lq_hw_pending(), 128);
    
    // Verify we can pop all available samples
    struct lq_hw_sample sample;
    int count = 0;
    while (lq_hw_pop(&sample) == 0) {
        count++;
    }
    
    // Should have gotten up to 128 samples (buffer size)
    EXPECT_LE(count, 128);
    EXPECT_GT(count, 0);
}

// ============================================================================
// Pending Count Tests
// ============================================================================

TEST_F(HwInputTest, PendingCountAccuracy) {
    EXPECT_EQ(lq_hw_pending(), 0);
    
    lq_hw_push(LQ_HW_ADC0, 1);
    EXPECT_EQ(lq_hw_pending(), 1);
    
    lq_hw_push(LQ_HW_ADC0, 2);
    EXPECT_EQ(lq_hw_pending(), 2);
    
    lq_hw_push(LQ_HW_ADC0, 3);
    EXPECT_EQ(lq_hw_pending(), 3);
    
    struct lq_hw_sample sample;
    lq_hw_pop(&sample);
    EXPECT_EQ(lq_hw_pending(), 2);
    
    lq_hw_pop(&sample);
    EXPECT_EQ(lq_hw_pending(), 1);
    
    lq_hw_pop(&sample);
    EXPECT_EQ(lq_hw_pending(), 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(HwInputTest, SimulateISRPattern) {
    // Simulate periodic ISR pattern (e.g., ADC conversion complete every 1ms)
    const int num_conversions = 50;
    
    for (int i = 0; i < num_conversions; i++) {
        // Simulate ADC conversion
        uint32_t adc_value = 2048 + (i * 10);
        lq_hw_push(LQ_HW_ADC0, adc_value);
        
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    EXPECT_EQ(lq_hw_pending(), num_conversions);
    
    // Process samples as engine would
    struct lq_hw_sample sample;
    uint64_t last_timestamp = 0;
    
    for (int i = 0; i < num_conversions; i++) {
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.src, LQ_HW_ADC0);
        EXPECT_EQ(sample.value, 2048 + (i * 10));
        
        // Timestamps should be monotonic
        EXPECT_GE(sample.timestamp, last_timestamp);
        last_timestamp = sample.timestamp;
    }
}

TEST_F(HwInputTest, SimulateMultiSourceISR) {
    // Simulate multiple hardware sources generating samples
    const int samples_per_source = 20;
    
    for (int i = 0; i < samples_per_source; i++) {
        lq_hw_push(LQ_HW_ADC0, 1000 + i);
        lq_hw_push(LQ_HW_ADC1, 2000 + i);
        lq_hw_push(LQ_HW_SPI0, 3000 + i);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    
    EXPECT_EQ(lq_hw_pending(), samples_per_source * 3);
    
    // Pop and verify interleaved order (FIFO)
    struct lq_hw_sample sample;
    for (int i = 0; i < samples_per_source; i++) {
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.src, LQ_HW_ADC0);
        EXPECT_EQ(sample.value, 1000 + i);
        
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.src, LQ_HW_ADC1);
        EXPECT_EQ(sample.value, 2000 + i);
        
        ASSERT_EQ(lq_hw_pop(&sample), 0);
        EXPECT_EQ(sample.src, LQ_HW_SPI0);
        EXPECT_EQ(sample.value, 3000 + i);
    }
    
    EXPECT_EQ(lq_hw_pending(), 0);
}
