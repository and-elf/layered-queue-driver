/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO Pattern Driver Unit Tests
 *
 * Comprehensive tests for GPIO pattern generation:
 * - Static patterns
 * - Blink patterns (50% duty cycle)
 * - PWM patterns (variable duty cycle)
 * - Custom bit patterns
 * - Signal-controlled patterns
 * - Timing accuracy
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "lq_gpio_pattern.h"
#include "lq_engine.h"
}

/* ============================================================================
 * Mock GPIO Functions
 * ============================================================================ */

static std::vector<uint8_t> gpio_set_pins;
static std::vector<bool> gpio_set_values;

extern "C" {
    // Mock lq_gpio_set function
    int lq_gpio_set(uint8_t pin, bool value) {
        gpio_set_pins.push_back(pin);
        gpio_set_values.push_back(value);
        return 0;
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GpioPatternTest : public ::testing::Test {
protected:
    struct lq_engine engine;
    struct lq_gpio_pattern_ctx ctx;

    void SetUp() override {
        // Clear mock data
        gpio_set_pins.clear();
        gpio_set_values.clear();

        // Initialize engine
        memset(&engine, 0, sizeof(engine));
        engine.num_signals = LQ_MAX_SIGNALS;
        lq_engine_init(&engine);

        // Initialize pattern context
        memset(&ctx, 0, sizeof(ctx));
    }

    void TearDown() override {
        // Cleanup if needed
    }

    // Helper: Get last GPIO state
    bool get_last_gpio_state() {
        if (gpio_set_values.empty()) {
            return false;
        }
        return gpio_set_values.back();
    }

    // Helper: Count GPIO changes
    size_t count_gpio_changes() {
        return gpio_set_pins.size();
    }

    // Helper: Set signal value
    void set_signal(uint8_t id, int32_t value) {
        engine.signals[id].value = value;
        engine.signals[id].status = LQ_EVENT_OK;
        engine.signals[id].timestamp = 1000000;
        engine.signals[id].updated = true;
    }
};

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST_F(GpioPatternTest, Init_Basic) {
    lq_gpio_pattern_init(&ctx, 5, LQ_GPIO_PATTERN_BLINK, 1000000);

    EXPECT_EQ(ctx.gpio_pin, 5);
    EXPECT_EQ(ctx.type, LQ_GPIO_PATTERN_BLINK);
    EXPECT_EQ(ctx.period_us, 1000000);
    EXPECT_EQ(ctx.on_time_us, 500000); // 50% duty cycle default
    EXPECT_EQ(ctx.control_signal, 0xFF); // No control signal
    EXPECT_TRUE(ctx.enabled);
    EXPECT_FALSE(ctx.inverted);
}

TEST_F(GpioPatternTest, Init_NullContext) {
    // Should not crash
    lq_gpio_pattern_init(nullptr, 5, LQ_GPIO_PATTERN_BLINK, 1000000);
    SUCCEED();
}

TEST_F(GpioPatternTest, SetDuty_Valid) {
    lq_gpio_pattern_init(&ctx, 5, LQ_GPIO_PATTERN_PWM, 1000000);

    lq_gpio_pattern_set_duty(&ctx, 25);
    EXPECT_EQ(ctx.on_time_us, 250000); // 25% of 1000000

    lq_gpio_pattern_set_duty(&ctx, 75);
    EXPECT_EQ(ctx.on_time_us, 750000); // 75% of 1000000

    lq_gpio_pattern_set_duty(&ctx, 0);
    EXPECT_EQ(ctx.on_time_us, 0);

    lq_gpio_pattern_set_duty(&ctx, 100);
    EXPECT_EQ(ctx.on_time_us, 1000000);
}

TEST_F(GpioPatternTest, SetDuty_Invalid) {
    lq_gpio_pattern_init(&ctx, 5, LQ_GPIO_PATTERN_PWM, 1000000);
    uint32_t original = ctx.on_time_us;

    // Should ignore invalid values
    lq_gpio_pattern_set_duty(&ctx, 101);
    EXPECT_EQ(ctx.on_time_us, original);
}

TEST_F(GpioPatternTest, SetCustom_Valid) {
    lq_gpio_pattern_init(&ctx, 5, LQ_GPIO_PATTERN_BLINK, 1000000);

    lq_gpio_pattern_set_custom(&ctx, 0xAAAAAAAA, 16);

    EXPECT_EQ(ctx.type, LQ_GPIO_PATTERN_CUSTOM);
    EXPECT_EQ(ctx.pattern_bits, 0xAAAAAAAA);
    EXPECT_EQ(ctx.pattern_length, 16);
    EXPECT_EQ(ctx.pattern_index, 0);
}

TEST_F(GpioPatternTest, SetCustom_Invalid) {
    lq_gpio_pattern_init(&ctx, 5, LQ_GPIO_PATTERN_BLINK, 1000000);

    // Invalid: length too large
    lq_gpio_pattern_set_custom(&ctx, 0xAAAAAAAA, 33);
    EXPECT_NE(ctx.type, LQ_GPIO_PATTERN_CUSTOM);

    // Invalid: length zero
    lq_gpio_pattern_set_custom(&ctx, 0xAAAAAAAA, 0);
    EXPECT_NE(ctx.type, LQ_GPIO_PATTERN_CUSTOM);
}

/* ============================================================================
 * Static Pattern Tests
 * ============================================================================ */

TEST_F(GpioPatternTest, Static_AlwaysHigh) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_STATIC, 1000000);

    // Process at different times
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_TRUE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 500000);
    EXPECT_TRUE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 1000000);
    EXPECT_TRUE(get_last_gpio_state());
}

TEST_F(GpioPatternTest, Static_Inverted) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_STATIC, 1000000);
    ctx.inverted = true;

    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_FALSE(get_last_gpio_state());
}

/* ============================================================================
 * Blink Pattern Tests
 * ============================================================================ */

TEST_F(GpioPatternTest, Blink_OneCycle) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000);

    // First half: should be high (50% duty)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_TRUE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 250000);
    EXPECT_TRUE(get_last_gpio_state());

    // Second half: should be low (at exactly 500000, we've crossed the boundary)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 500000);
    EXPECT_FALSE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 750000);
    EXPECT_FALSE(get_last_gpio_state());

    // Wrap around: should be high again (phase wraps back to 0)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 1000000);
    EXPECT_TRUE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 1250000);
    EXPECT_TRUE(get_last_gpio_state());
}

TEST_F(GpioPatternTest, Blink_Frequency1Hz) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000); // 1 Hz = 1M us

    size_t changes = 0;
    bool last_state = false;

    // Run for 5 seconds with 10ms steps
    for (uint64_t t = 0; t <= 5000000; t += 10000) {
        lq_process_gpio_patterns(nullptr, &ctx, 1, t);
        bool current_state = get_last_gpio_state();
        if (current_state != last_state) {
            changes++;
            last_state = current_state;
        }
    }

    // Should have ~10 transitions (5 seconds * 2 transitions per second)
    EXPECT_GE(changes, 9);
    EXPECT_LE(changes, 11);
}

/* ============================================================================
 * PWM Pattern Tests
 * ============================================================================ */

TEST_F(GpioPatternTest, PWM_25PercentDuty) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_PWM, 1000000);
    lq_gpio_pattern_set_duty(&ctx, 25);

    // First 25%: high
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_TRUE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 200000);
    EXPECT_TRUE(get_last_gpio_state());

    // After 25%: low
    lq_process_gpio_patterns(nullptr, &ctx, 1, 300000);
    EXPECT_FALSE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 900000);
    EXPECT_FALSE(get_last_gpio_state());
}

TEST_F(GpioPatternTest, PWM_75PercentDuty) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_PWM, 1000000);
    lq_gpio_pattern_set_duty(&ctx, 75);

    // First 75%: high
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_TRUE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 700000);
    EXPECT_TRUE(get_last_gpio_state());

    // After 75%: low
    lq_process_gpio_patterns(nullptr, &ctx, 1, 800000);
    EXPECT_FALSE(get_last_gpio_state());
}

/* ============================================================================
 * Custom Pattern Tests
 * ============================================================================ */

TEST_F(GpioPatternTest, Custom_AlternatingBits) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_CUSTOM, 1000000);
    lq_gpio_pattern_set_custom(&ctx, 0b10101010, 8); // Alternating: bits are 01010101

    // Period 0 (t=0): Bit 0 = 0 (FALSE)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_FALSE(get_last_gpio_state());

    // Period 1 (t=1000000): Bit 1 = 1 (TRUE)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 1000000);
    EXPECT_TRUE(get_last_gpio_state());

    // Period 2 (t=2000000): Bit 2 = 0 (FALSE)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 2000000);
    EXPECT_FALSE(get_last_gpio_state());

    // Period 3 (t=3000000): Bit 3 = 1 (TRUE)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 3000000);
    EXPECT_TRUE(get_last_gpio_state());
}

TEST_F(GpioPatternTest, Custom_ThreeFlashes) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_CUSTOM, 100000);
    // Pattern: 111000111000111000000000 (3 flashes, then long pause)
    lq_gpio_pattern_set_custom(&ctx, 0b000000111000111000111, 24);

    // First 3 bits: all high
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_TRUE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 100000);
    EXPECT_TRUE(get_last_gpio_state());

    lq_process_gpio_patterns(nullptr, &ctx, 1, 200000);
    EXPECT_TRUE(get_last_gpio_state());

    // Next 3 bits: all low
    lq_process_gpio_patterns(nullptr, &ctx, 1, 300000);
    EXPECT_FALSE(get_last_gpio_state());
}

TEST_F(GpioPatternTest, Custom_PatternWraparound) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_CUSTOM, 100000);
    lq_gpio_pattern_set_custom(&ctx, 0b101, 3); // 3-bit pattern

    // Bit 0: 1
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_TRUE(get_last_gpio_state());

    // Bit 1: 0
    lq_process_gpio_patterns(nullptr, &ctx, 1, 100000);
    EXPECT_FALSE(get_last_gpio_state());

    // Bit 2: 1
    lq_process_gpio_patterns(nullptr, &ctx, 1, 200000);
    EXPECT_TRUE(get_last_gpio_state());

    // Bit 0 again (wrap): 1
    lq_process_gpio_patterns(nullptr, &ctx, 1, 300000);
    EXPECT_TRUE(get_last_gpio_state());
}

/* ============================================================================
 * Signal Control Tests
 * ============================================================================ */

TEST_F(GpioPatternTest, SignalControl_EnableDisable) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000);
    ctx.control_signal = 5; // Use signal 5 as control

    // Signal = 0: pattern disabled
    set_signal(5, 0);
    lq_process_gpio_patterns(&engine, &ctx, 1, 0);
    lq_process_gpio_patterns(&engine, &ctx, 1, 100000);

    // Should not have updated GPIO (or updated to low)
    bool initial_calls = count_gpio_changes();

    // Signal = 1: pattern enabled
    set_signal(5, 1);
    lq_process_gpio_patterns(&engine, &ctx, 1, 200000);

    // Should have updated GPIO
    EXPECT_GT(count_gpio_changes(), initial_calls);
}

TEST_F(GpioPatternTest, SignalControl_NonZeroEnables) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000);
    ctx.control_signal = 5;

    // Any non-zero value should enable
    set_signal(5, 100);
    lq_process_gpio_patterns(&engine, &ctx, 1, 0);
    EXPECT_TRUE(get_last_gpio_state());

    set_signal(5, -50);
    lq_process_gpio_patterns(&engine, &ctx, 1, 100000);
    EXPECT_TRUE(get_last_gpio_state());
}

/* ============================================================================
 * Enable/Disable Tests
 * ============================================================================ */

TEST_F(GpioPatternTest, Enable_Disable) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000);

    // Start enabled
    EXPECT_TRUE(ctx.enabled);

    // Disable
    lq_gpio_pattern_enable(&ctx, false);
    EXPECT_FALSE(ctx.enabled);

    // Should turn off GPIO when disabled
    EXPECT_EQ(gpio_set_pins.back(), 10);
    EXPECT_FALSE(gpio_set_values.back());

    // Process should do nothing when disabled
    size_t calls_before = count_gpio_changes();
    lq_process_gpio_patterns(nullptr, &ctx, 1, 100000);
    EXPECT_EQ(count_gpio_changes(), calls_before);

    // Re-enable
    lq_gpio_pattern_enable(&ctx, true);
    EXPECT_TRUE(ctx.enabled);
}

/* ============================================================================
 * Multiple Pattern Tests
 * ============================================================================ */

TEST_F(GpioPatternTest, MultiplePatterns_Independent) {
    struct lq_gpio_pattern_ctx patterns[3];

    lq_gpio_pattern_init(&patterns[0], 10, LQ_GPIO_PATTERN_BLINK, 1000000);
    lq_gpio_pattern_init(&patterns[1], 11, LQ_GPIO_PATTERN_BLINK, 500000);
    lq_gpio_pattern_init(&patterns[2], 12, LQ_GPIO_PATTERN_STATIC, 1000000);

    lq_process_gpio_patterns(nullptr, patterns, 3, 0);

    // All should be processed
    EXPECT_GE(count_gpio_changes(), 3);
}

TEST_F(GpioPatternTest, MultiplePatterns_DifferentFrequencies) {
    struct lq_gpio_pattern_ctx patterns[2];

    // 1Hz pattern
    lq_gpio_pattern_init(&patterns[0], 10, LQ_GPIO_PATTERN_BLINK, 1000000);

    // 2Hz pattern (twice as fast)
    lq_gpio_pattern_init(&patterns[1], 11, LQ_GPIO_PATTERN_BLINK, 500000);

    int changes_p0 = 0, changes_p1 = 0;
    bool last_state_p0 = false, last_state_p1 = false;

    // Run for 2 seconds
    for (uint64_t t = 0; t <= 2000000; t += 10000) {
        gpio_set_pins.clear();
        gpio_set_values.clear();

        lq_process_gpio_patterns(nullptr, patterns, 2, t);

        // Track state changes per pin
        for (size_t i = 0; i < gpio_set_pins.size(); i++) {
            if (gpio_set_pins[i] == 10 && gpio_set_values[i] != last_state_p0) {
                changes_p0++;
                last_state_p0 = gpio_set_values[i];
            }
            if (gpio_set_pins[i] == 11 && gpio_set_values[i] != last_state_p1) {
                changes_p1++;
                last_state_p1 = gpio_set_values[i];
            }
        }
    }

    // 2Hz pattern should have approximately twice as many changes as 1Hz
    EXPECT_GT(changes_p1, changes_p0);
}

/* ============================================================================
 * Edge Cases and Error Handling
 * ============================================================================ */

TEST_F(GpioPatternTest, NullEngine_WithoutControl) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000);

    // Should work fine without engine if no control signal
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_GT(count_gpio_changes(), 0);
}

TEST_F(GpioPatternTest, NullPatterns) {
    // Should not crash
    lq_process_gpio_patterns(nullptr, nullptr, 1, 0);
    SUCCEED();
}

TEST_F(GpioPatternTest, ZeroPatterns) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000);

    // Should do nothing
    size_t calls_before = count_gpio_changes();
    lq_process_gpio_patterns(nullptr, &ctx, 0, 0);
    EXPECT_EQ(count_gpio_changes(), calls_before);
}

TEST_F(GpioPatternTest, InvalidControlSignal) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000);
    ctx.control_signal = 255; // Out of range

    // First call initializes (writes initial state)
    lq_process_gpio_patterns(&engine, &ctx, 1, 0);
    size_t calls_after_init = count_gpio_changes();
    EXPECT_EQ(calls_after_init, 1); // One call for initialization

    // Subsequent calls should skip processing due to invalid signal
    lq_process_gpio_patterns(&engine, &ctx, 1, 100000);
    EXPECT_EQ(count_gpio_changes(), calls_after_init); // No additional calls
}

TEST_F(GpioPatternTest, Inverted_Blink) {
    lq_gpio_pattern_init(&ctx, 10, LQ_GPIO_PATTERN_BLINK, 1000000);
    ctx.inverted = true;

    // First half: should be LOW (inverted)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 0);
    EXPECT_FALSE(get_last_gpio_state());

    // Second half: should be HIGH (inverted)
    lq_process_gpio_patterns(nullptr, &ctx, 1, 600000);
    EXPECT_TRUE(get_last_gpio_state());
}
