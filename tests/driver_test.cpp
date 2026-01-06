/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Driver Unit Tests
 * 
 * Comprehensive tests for all mid-level drivers to achieve full coverage:
 * - Remap (deadzone, inversion)
 * - Scale (linear transform, clamping)
 * - Verified Output (command vs feedback)
 * - PID Controller (P, I, D, anti-windup)
 * - Fault Monitor (with limp-home integration)
 */

#include <gtest/gtest.h>
#include "lq_engine.h"
#include "lq_remap.h"
#include "lq_scale.h"
#include "lq_verified_output.h"
#include "lq_pid.h"
#include "layered_queue_core.h"
#include <limits.h>

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class DriverTest : public ::testing::Test {
protected:
    struct lq_engine engine;
    
    void SetUp() override {
        memset(&engine, 0, sizeof(engine));
        engine.num_signals = LQ_MAX_SIGNALS;
        lq_engine_init(&engine);
    }
    
    void TearDown() override {
        // Cleanup if needed
    }
    
    // Helper: Set signal value
    void set_signal(uint8_t id, int32_t value, enum lq_event_status status = LQ_EVENT_OK) {
        engine.signals[id].value = value;
        engine.signals[id].status = status;
        engine.signals[id].timestamp = 1000000; // 1 second
        engine.signals[id].updated = true;
    }
    
    // Helper: Get signal value
    int32_t get_signal(uint8_t id) {
        return engine.signals[id].value;
    }
    
    // Helper: Get signal status
    enum lq_event_status get_status(uint8_t id) {
        return engine.signals[id].status;
    }
};

/* ============================================================================
 * Remap Driver Tests
 * ============================================================================ */

TEST_F(DriverTest, Remap_BasicMapping) {
    struct lq_remap_ctx remap;
    remap.input_signal = 0;
    remap.output_signal = 1;
    remap.invert = false;
    remap.deadzone = 0;
    remap.enabled = true;
    
    set_signal(0, 1000);
    
    lq_process_remaps(&engine, &remap, 1, 1000000);
    
    EXPECT_EQ(get_signal(1), 1000);
    EXPECT_EQ(get_status(1), LQ_EVENT_OK);
}

TEST_F(DriverTest, Remap_Inversion) {
    struct lq_remap_ctx remap;
    remap.input_signal = 0;
    remap.output_signal = 1;
    remap.invert = true;
    remap.deadzone = 0;
    remap.enabled = true;
    
    set_signal(0, 500);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), -500);
    
    set_signal(0, -200);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), 200);
}

TEST_F(DriverTest, Remap_Deadzone) {
    struct lq_remap_ctx remap;
    remap.input_signal = 0;
    remap.output_signal = 1;
    remap.invert = false;
    remap.deadzone = 100;
    remap.enabled = true;
    
    // Within deadzone - should be zero
    set_signal(0, 50);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), 0);
    
    set_signal(0, -99);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), 0);
    
    // At deadzone boundary
    set_signal(0, 100);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), 0);
    
    // Outside deadzone - passes through
    set_signal(0, 101);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), 101);
    
    set_signal(0, -150);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), -150);
}

TEST_F(DriverTest, Remap_DeadzoneWithInversion) {
    struct lq_remap_ctx remap;
    remap.input_signal = 0;
    remap.output_signal = 1;
    remap.invert = true;
    remap.deadzone = 50;
    remap.enabled = true;
    
    // Deadzone applied before inversion
    set_signal(0, 30);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), 0);
    
    set_signal(0, 100);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    EXPECT_EQ(get_signal(1), -100);
}

TEST_F(DriverTest, Remap_ErrorPropagation) {
    struct lq_remap_ctx remap;
    remap.input_signal = 0;
    remap.output_signal = 1;
    remap.invert = false;
    remap.deadzone = 0;
    remap.enabled = true;
    
    set_signal(0, 500, LQ_EVENT_ERROR);
    lq_process_remaps(&engine, &remap, 1, 1000000);
    
    EXPECT_EQ(get_signal(1), 500);
    EXPECT_EQ(get_status(1), LQ_EVENT_ERROR);
}

TEST_F(DriverTest, Remap_Disabled) {
    struct lq_remap_ctx remap;
    remap.input_signal = 0;
    remap.output_signal = 1;
    remap.invert = true;
    remap.deadzone = 0;
    remap.enabled = false; // Disabled
    
    set_signal(0, 1000);
    set_signal(1, 9999); // Pre-existing value
    
    lq_process_remaps(&engine, &remap, 1, 1000000);
    
    // Should not modify output
    EXPECT_EQ(get_signal(1), 9999);
}

/* ============================================================================
 * Scale Driver Tests
 * ============================================================================ */

TEST_F(DriverTest, Scale_BasicScaling) {
    struct lq_scale_ctx scale;
    scale.input_signal = 0;
    scale.output_signal = 1;
    scale.scale_factor = 2000; // 2.0x
    scale.offset = 0;
    scale.has_clamp_min = false;
    scale.has_clamp_max = false;
    scale.enabled = true;
    
    set_signal(0, 100);
    lq_process_scales(&engine, &scale, 1, 1000000);
    
    EXPECT_EQ(get_signal(1), 200); // 100 * 2.0
}

TEST_F(DriverTest, Scale_WithOffset) {
    struct lq_scale_ctx scale;
    scale.input_signal = 0;
    scale.output_signal = 1;
    scale.scale_factor = 1000; // 1.0x
    scale.offset = 500;
    scale.has_clamp_min = false;
    scale.has_clamp_max = false;
    scale.enabled = true;
    
    set_signal(0, 100);
    lq_process_scales(&engine, &scale, 1, 1000000);
    
    EXPECT_EQ(get_signal(1), 600); // (100 * 1.0) + 500
}

TEST_F(DriverTest, Scale_Clamping) {
    struct lq_scale_ctx scale;
    scale.input_signal = 0;
    scale.output_signal = 1;
    scale.scale_factor = 1000;
    scale.offset = 0;
    scale.clamp_min = -100;
    scale.clamp_max = 100;
    scale.has_clamp_min = true;
    scale.has_clamp_max = true;
    scale.enabled = true;
    
    // Within range
    set_signal(0, 50);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), 50);
    
    // Above max - clamp
    set_signal(0, 200);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), 100);
    
    // Below min - clamp
    set_signal(0, -500);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), -100);
}

TEST_F(DriverTest, Scale_OnlyMaxClamp) {
    struct lq_scale_ctx scale;
    scale.input_signal = 0;
    scale.output_signal = 1;
    scale.scale_factor = 1000;
    scale.offset = 0;
    scale.clamp_max = 1000;
    scale.has_clamp_min = false;
    scale.has_clamp_max = true;
    scale.enabled = true;
    
    set_signal(0, 500);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), 500);
    
    set_signal(0, 2000);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), 1000);
    
    set_signal(0, -5000); // No min clamp
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), -5000);
}

TEST_F(DriverTest, Scale_NegativeScaleFactor) {
    struct lq_scale_ctx scale;
    scale.input_signal = 0;
    scale.output_signal = 1;
    scale.scale_factor = -1000; // -1.0x (inversion)
    scale.offset = 0;
    scale.has_clamp_min = false;
    scale.has_clamp_max = false;
    scale.enabled = true;
    
    set_signal(0, 100);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), -100);
}

TEST_F(DriverTest, Scale_FractionalScaling) {
    struct lq_scale_ctx scale;
    scale.input_signal = 0;
    scale.output_signal = 1;
    scale.scale_factor = 500; // 0.5x
    scale.offset = 0;
    scale.has_clamp_min = false;
    scale.has_clamp_max = false;
    scale.enabled = true;
    
    set_signal(0, 1000);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), 500); // 1000 * 0.5
}

TEST_F(DriverTest, Scale_Saturation) {
    struct lq_scale_ctx scale;
    scale.input_signal = 0;
    scale.output_signal = 1;
    scale.scale_factor = 1000000; // Very large
    scale.offset = 0;
    scale.has_clamp_min = false;
    scale.has_clamp_max = false;
    scale.enabled = true;
    
    // Use very large input to cause saturation
    // 3000000 * 1000000 / 1000 = 3000000000 > INT32_MAX
    set_signal(0, 3000000);
    lq_process_scales(&engine, &scale, 1, 1000000);
    
    // Should saturate to INT32_MAX
    EXPECT_EQ(get_signal(1), INT32_MAX);
}

/* ============================================================================
 * Verified Output Tests
 * ============================================================================ */

TEST_F(DriverTest, VerifiedOutput_ImmediateMatch) {
    struct lq_verified_output_ctx vo;
    vo.command_signal = 0;
    vo.verification_signal = 1;
    vo.output_signal = 2;
    vo.output_type = LQ_VERIFIED_GPIO;
    vo.tolerance = 0;
    vo.verify_timeout_us = 0; // Immediate
    vo.continuous_verify = true;
    vo.waiting_for_verify = false;
    vo.last_command = -1; // Force initial state
    vo.enabled = true;
    
    set_signal(0, 1); // Command
    set_signal(1, 1); // Verification matches
    
    lq_process_verified_outputs(&engine, &vo, 1, 1000000);
    
    EXPECT_EQ(get_signal(2), 1); // Output = verification value
    EXPECT_EQ(get_status(2), LQ_EVENT_OK); // Verified
}

TEST_F(DriverTest, VerifiedOutput_Mismatch) {
    struct lq_verified_output_ctx vo;
    vo.command_signal = 0;
    vo.verification_signal = 1;
    vo.output_signal = 2;
    vo.output_type = LQ_VERIFIED_GPIO;
    vo.tolerance = 0;
    vo.verify_timeout_us = 0;
    vo.continuous_verify = true;
    vo.waiting_for_verify = false;
    vo.last_command = -1;
    vo.enabled = true;
    
    set_signal(0, 1); // Command ON
    set_signal(1, 0); // Verification OFF - MISMATCH!
    
    lq_process_verified_outputs(&engine, &vo, 1, 1000000);
    
    EXPECT_EQ(get_signal(2), 0); // Output shows actual value
    EXPECT_EQ(get_status(2), LQ_EVENT_ERROR); // FAULT
}

TEST_F(DriverTest, VerifiedOutput_WithTolerance) {
    struct lq_verified_output_ctx vo;
    vo.command_signal = 0;
    vo.verification_signal = 1;
    vo.output_signal = 2;
    vo.output_type = LQ_VERIFIED_ANALOG;
    vo.tolerance = 50; // ±50 tolerance
    vo.verify_timeout_us = 0;
    vo.continuous_verify = true;
    vo.waiting_for_verify = false;
    vo.last_command = -1;
    vo.enabled = true;
    
    // Within tolerance
    set_signal(0, 1000);
    set_signal(1, 1040);
    lq_process_verified_outputs(&engine, &vo, 1, 1000000);
    EXPECT_EQ(get_status(2), LQ_EVENT_OK);
    
    // At tolerance boundary
    set_signal(1, 1050);
    lq_process_verified_outputs(&engine, &vo, 1, 1000000);
    EXPECT_EQ(get_status(2), LQ_EVENT_OK);
    
    // Outside tolerance
    set_signal(1, 1051);
    lq_process_verified_outputs(&engine, &vo, 1, 1000000);
    EXPECT_EQ(get_status(2), LQ_EVENT_ERROR);
}

TEST_F(DriverTest, VerifiedOutput_Timeout) {
    struct lq_verified_output_ctx vo;
    vo.command_signal = 0;
    vo.verification_signal = 1;
    vo.output_signal = 2;
    vo.output_type = LQ_VERIFIED_GPIO;
    vo.tolerance = 0;
    vo.verify_timeout_us = 100000; // 100ms timeout
    vo.continuous_verify = false; // One-shot
    vo.waiting_for_verify = false;
    vo.last_command = -1;
    vo.command_timestamp = 0;
    vo.enabled = true;
    
    set_signal(0, 1);
    set_signal(1, 1);
    
    // First call - command changes, start waiting
    lq_process_verified_outputs(&engine, &vo, 1, 1000000);
    EXPECT_TRUE(vo.waiting_for_verify);
    EXPECT_EQ(get_status(2), LQ_EVENT_OK); // Not verified yet, but not faulted
    
    // Before timeout expires - still waiting
    lq_process_verified_outputs(&engine, &vo, 1, 1050000);
    EXPECT_TRUE(vo.waiting_for_verify);
    
    // After timeout - verify
    lq_process_verified_outputs(&engine, &vo, 1, 1100000);
    EXPECT_FALSE(vo.waiting_for_verify);
    EXPECT_EQ(get_status(2), LQ_EVENT_OK); // Verified
}

TEST_F(DriverTest, VerifiedOutput_CommandChange) {
    struct lq_verified_output_ctx vo;
    vo.command_signal = 0;
    vo.verification_signal = 1;
    vo.output_signal = 2;
    vo.output_type = LQ_VERIFIED_GPIO;
    vo.tolerance = 0;
    vo.verify_timeout_us = 50000;
    vo.continuous_verify = true;
    vo.waiting_for_verify = false;
    vo.last_command = 0;
    vo.command_timestamp = 1000000;
    vo.enabled = true;
    
    set_signal(0, 0);
    set_signal(1, 0);
    
    // Command changes
    set_signal(0, 1);
    lq_process_verified_outputs(&engine, &vo, 1, 2000000);
    
    EXPECT_EQ(vo.last_command, 1);
    EXPECT_EQ(vo.command_timestamp, 2000000ULL);
    EXPECT_TRUE(vo.waiting_for_verify);
}

/* ============================================================================
 * PID Controller Tests
 * ============================================================================ */

TEST_F(DriverTest, PID_ProportionalOnly) {
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 0;
    pid.measurement_signal = 1;
    pid.output_signal = 2;
    pid.kp = 10000; // Kp = 10.0
    pid.ki = 0;
    pid.kd = 0;
    pid.output_min = -1000;
    pid.output_max = 1000;
    pid.integral_min = -1000000;
    pid.integral_max = 1000000;
    pid.deadband = 0;
    pid.sample_time_us = 0;
    pid.reset_on_setpoint_change = true;
    pid.first_run = true;
    pid.enabled = true;
    
    set_signal(0, 100); // Setpoint
    set_signal(1, 90);  // Measurement
    
    // First run - initialization
    lq_process_pids(&engine, &pid, 1, 1000000);
    EXPECT_FALSE(pid.first_run);
    
    // Second run - calculate P term
    lq_process_pids(&engine, &pid, 1, 1100000);
    
    // error = 100 - 90 = 10
    // P = 10.0 * 10 = 100
    EXPECT_EQ(get_signal(2), 100);
}

TEST_F(DriverTest, PID_IntegralAccumulation) {
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 0;
    pid.measurement_signal = 1;
    pid.output_signal = 2;
    pid.kp = 1000; // Kp = 1.0
    pid.ki = 1000; // Ki = 1.0
    pid.kd = 0;
    pid.output_min = -10000;
    pid.output_max = 10000;
    pid.integral_min = -1000000;
    pid.integral_max = 1000000;
    pid.deadband = 0;
    pid.sample_time_us = 100000; // Fixed 100ms
    pid.reset_on_setpoint_change = true;
    pid.integral = 0;
    pid.first_run = true;
    pid.enabled = true;
    
    set_signal(0, 100); // Setpoint
    set_signal(1, 90);  // Measurement (error = 10)
    
    lq_process_pids(&engine, &pid, 1, 0);
    lq_process_pids(&engine, &pid, 1, 100000); // dt = 100ms = 0.1s
    
    // error = 10
    // P = 1.0 * 10 = 10
    // I = 1.0 * (10 * 0.1) = 1.0
    int32_t output1 = get_signal(2);
    EXPECT_NEAR(output1, 11, 1); // P + I ≈ 11
    
    // Run again - integral should accumulate
    lq_process_pids(&engine, &pid, 1, 200000);
    int32_t output2 = get_signal(2);
    EXPECT_GT(output2, output1); // Should be larger due to accumulation
}

TEST_F(DriverTest, PID_IntegralAntiWindup) {
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 0;
    pid.measurement_signal = 1;
    pid.output_signal = 2;
    pid.kp = 0;
    pid.ki = 1000000; // Very large Ki to force windup
    pid.kd = 0;
    pid.output_min = -1000;
    pid.output_max = 1000;
    pid.integral_min = -100; // Tight anti-windup limit
    pid.integral_max = 100;
    pid.deadband = 0;
    pid.sample_time_us = 100000;
    pid.reset_on_setpoint_change = true;
    pid.integral = 0;
    pid.first_run = true;
    pid.enabled = true;
    
    set_signal(0, 1000);
    set_signal(1, 0); // Large error
    
    lq_process_pids(&engine, &pid, 1, 0);
    lq_process_pids(&engine, &pid, 1, 100000);
    
    // Integral should be clamped
    EXPECT_LE(pid.integral, 100);
    EXPECT_GE(pid.integral, -100);
}

TEST_F(DriverTest, PID_Deadband) {
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 0;
    pid.measurement_signal = 1;
    pid.output_signal = 2;
    pid.kp = 1000;
    pid.ki = 1000;
    pid.kd = 0;
    pid.output_min = -1000;
    pid.output_max = 1000;
    pid.integral_min = -1000000;
    pid.integral_max = 1000000;
    pid.deadband = 10; // ±10 deadband
    pid.sample_time_us = 100000;
    pid.reset_on_setpoint_change = true;
    pid.integral = 0;
    pid.first_run = true;
    pid.enabled = true;
    
    set_signal(0, 100);
    set_signal(1, 95); // error = 5, within deadband
    
    lq_process_pids(&engine, &pid, 1, 0);
    lq_process_pids(&engine, &pid, 1, 100000);
    
    int64_t integral_before = pid.integral;
    
    // Run again - integral should NOT accumulate (within deadband)
    lq_process_pids(&engine, &pid, 1, 200000);
    EXPECT_EQ(pid.integral, integral_before);
}

TEST_F(DriverTest, PID_Derivative) {
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 0;
    pid.measurement_signal = 1;
    pid.output_signal = 2;
    pid.kp = 0;
    pid.ki = 0;
    pid.kd = 1000; // Kd = 1.0
    pid.output_min = -10000;
    pid.output_max = 10000;
    pid.integral_min = -1000000;
    pid.integral_max = 1000000;
    pid.deadband = 0;
    pid.sample_time_us = 100000; // 0.1s
    pid.reset_on_setpoint_change = true;
    pid.last_error = 0;
    pid.first_run = true;
    pid.enabled = true;
    
    set_signal(0, 100);
    set_signal(1, 90); // error = 10
    
    lq_process_pids(&engine, &pid, 1, 0);
    lq_process_pids(&engine, &pid, 1, 100000);
    
    // Change measurement - error changes
    set_signal(1, 80); // error = 20, delta = 10
    lq_process_pids(&engine, &pid, 1, 200000);
    
    // D = Kd * d(error)/dt = 1.0 * (10 / 0.1) = 100
    EXPECT_NEAR(get_signal(2), 100, 10);
}

TEST_F(DriverTest, PID_SetpointChange) {
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 0;
    pid.measurement_signal = 1;
    pid.output_signal = 2;
    pid.kp = 1000;
    pid.ki = 100; // Smaller Ki to see the reset effect
    pid.kd = 0;
    pid.output_min = -10000;
    pid.output_max = 10000;
    pid.integral_min = -1000000;
    pid.integral_max = 1000000;
    pid.deadband = 0;
    pid.sample_time_us = 100000;
    pid.reset_on_setpoint_change = true; // Should reset integral
    pid.integral = 0;
    pid.last_setpoint = 100;
    pid.first_run = false; // Already initialized
    pid.last_error = 0;
    pid.last_time = 0;
    pid.enabled = true;
    
    // First call: setpoint=100, measurement=90, error=10
    set_signal(0, 100);
    set_signal(1, 90);
    
    lq_process_pids(&engine, &pid, 1, 100000);
    
    // Integral should have accumulated
    int32_t integral_before_reset = pid.integral;
    EXPECT_GT(integral_before_reset, 0);
    
    // Change setpoint - integral should reset and then accumulate the new error
    set_signal(0, 200);
    lq_process_pids(&engine, &pid, 1, 200000);
    
    // After reset, new error is 200-90=110, much larger than before
    // So output should reflect the new error (P term) without the old integral
    int32_t output_after_reset = get_signal(2);
    // P term = 1000 * 110 / 1000 = 110
    // I term = 100 * (accumulated over 100ms) / 1000 = small
    // Output should be dominated by P term
    EXPECT_GE(output_after_reset, 100); // At least the P term
}

TEST_F(DriverTest, PID_OutputClamping) {
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 0;
    pid.measurement_signal = 1;
    pid.output_signal = 2;
    pid.kp = 1000000; // Huge gain to force saturation
    pid.ki = 0;
    pid.kd = 0;
    pid.output_min = -100;
    pid.output_max = 100;
    pid.integral_min = -1000000;
    pid.integral_max = 1000000;
    pid.deadband = 0;
    pid.sample_time_us = 100000;
    pid.reset_on_setpoint_change = true;
    pid.first_run = true;
    pid.enabled = true;
    
    set_signal(0, 1000);
    set_signal(1, 0);
    
    lq_process_pids(&engine, &pid, 1, 0);
    lq_process_pids(&engine, &pid, 1, 100000);
    
    // Should clamp to max
    EXPECT_EQ(get_signal(2), 100);
    
    // Negative direction
    set_signal(0, 0);
    set_signal(1, 1000);
    lq_process_pids(&engine, &pid, 1, 200000);
    
    EXPECT_EQ(get_signal(2), -100);
}

/* ============================================================================
 * Fault Monitor with Limp-Home Tests
 * ============================================================================ */

static int wake_call_count = 0;
static uint8_t wake_monitor_id = 0;
static int32_t wake_value = 0;
static enum lq_fault_level wake_level = LQ_FAULT_LEVEL_0;

void test_wake_function(uint8_t monitor_id, int32_t value, enum lq_fault_level level) {
    wake_call_count++;
    wake_monitor_id = monitor_id;
    wake_value = value;
    wake_level = level;
}

TEST_F(DriverTest, FaultMonitor_RangeCheck) {
    engine.fault_monitors[0].input_signal = 0;
    engine.fault_monitors[0].fault_output_signal = 1;
    engine.fault_monitors[0].check_staleness = false;
    engine.fault_monitors[0].check_range = true;
    engine.fault_monitors[0].min_value = 0;
    engine.fault_monitors[0].max_value = 100;
    engine.fault_monitors[0].check_status = false;
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_1;
    engine.fault_monitors[0].wake = nullptr;
    engine.fault_monitors[0].has_limp_action = false;
    engine.fault_monitors[0].enabled = true;
    engine.num_fault_monitors = 1;
    
    // Within range
    set_signal(0, 50);
    lq_process_fault_monitors(&engine, 1000000);
    EXPECT_EQ(get_signal(1), 0); // No fault
    
    // Above range
    set_signal(0, 150);
    lq_process_fault_monitors(&engine, 1000000);
    EXPECT_EQ(get_signal(1), 1); // Fault level 1
    
    // Below range
    set_signal(0, -10);
    lq_process_fault_monitors(&engine, 1000000);
    EXPECT_EQ(get_signal(1), 1); // Fault
}

TEST_F(DriverTest, FaultMonitor_WakeFunction) {
    wake_call_count = 0;
    
    struct lq_fault_monitor_ctx monitor;
    monitor.input_signal = 0;
    monitor.fault_output_signal = 1;
    monitor.check_staleness = false;
    monitor.check_range = true;
    monitor.min_value = 0;
    monitor.max_value = 100;
    monitor.check_status = false;
    monitor.fault_level = LQ_FAULT_LEVEL_2;
    monitor.wake = test_wake_function;
    monitor.has_limp_action = false;
    monitor.enabled = true;
    
    engine.fault_monitors[0] = monitor;
    engine.num_fault_monitors = 1;
    
    // Trigger fault via INGESTION (for immediate wake on raw values)
    struct lq_event event;
    event.source_id = 0;
    event.value = 200; // Out of range
    event.status = LQ_EVENT_OK;
    event.timestamp = 1000000;
    
    lq_ingest_events(&engine, &event, 1);
    
    // Wake should have been called during ingestion
    EXPECT_EQ(wake_call_count, 1);
    EXPECT_EQ(wake_monitor_id, 0);
    EXPECT_EQ(wake_value, 200);
    EXPECT_EQ(wake_level, LQ_FAULT_LEVEL_2);
}

TEST_F(DriverTest, FaultMonitor_LimpHomeActivation) {
    // Setup scale driver
    engine.scales[0].input_signal = 10;
    engine.scales[0].output_signal = 11;
    engine.scales[0].scale_factor = 1000;
    engine.scales[0].offset = 0;
    engine.scales[0].clamp_min = 0;
    engine.scales[0].clamp_max = 2000; // Normal: 0-2000
    engine.scales[0].has_clamp_min = true;
    engine.scales[0].has_clamp_max = true;
    engine.scales[0].enabled = true;
    engine.num_scales = 1;
    
    // Setup fault monitor with limp action
    engine.fault_monitors[0].input_signal = 0;
    engine.fault_monitors[0].fault_output_signal = 1;
    engine.fault_monitors[0].check_staleness = false;
    engine.fault_monitors[0].check_range = true;
    engine.fault_monitors[0].min_value = 0;
    engine.fault_monitors[0].max_value = 100;
    engine.fault_monitors[0].check_status = false;
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_1;
    engine.fault_monitors[0].wake = nullptr;
    engine.fault_monitors[0].has_limp_action = true;
    engine.fault_monitors[0].limp_target_scale_id = 0;
    engine.fault_monitors[0].limp_scale_factor = 500; // Reduce to 0.5x
    engine.fault_monitors[0].limp_clamp_max = 1000;   // Reduce to 0-1000
    engine.fault_monitors[0].limp_clamp_min = INT32_MIN; // Don't change min
    engine.fault_monitors[0].restore_delay_ms = 100;
    engine.fault_monitors[0].limp_active = false;
    engine.fault_monitors[0].enabled = true;
    engine.num_fault_monitors = 1;
    
    // No fault initially - normal operation
    set_signal(0, 50);
    lq_process_fault_monitors(&engine, 1000000);
    EXPECT_FALSE(engine.fault_monitors[0].limp_active);
    EXPECT_EQ(engine.scales[0].clamp_max, 2000); // Normal
    
    // Trigger fault - limp mode activates
    set_signal(0, 150); // Out of range
    lq_process_fault_monitors(&engine, 2000000);
    
    EXPECT_TRUE(engine.fault_monitors[0].limp_active);
    EXPECT_EQ(engine.scales[0].scale_factor, 500); // Limp value
    EXPECT_EQ(engine.scales[0].clamp_max, 1000);   // Limp value
    EXPECT_EQ(get_signal(1), 1); // Fault signal set
    
    // Note: Restoration timing relies on lq_platform_uptime_get() which uses real time,
    // so we don't test the delayed restoration in this unit test
}

TEST_F(DriverTest, FaultMonitor_StatusCheck) {
    struct lq_fault_monitor_ctx monitor;
    monitor.input_signal = 0;
    monitor.fault_output_signal = 1;
    monitor.check_staleness = false;
    monitor.check_range = false;
    monitor.check_status = true;
    monitor.fault_level = LQ_FAULT_LEVEL_3;
    monitor.wake = nullptr;
    monitor.has_limp_action = false;
    monitor.enabled = true;
    
    engine.fault_monitors[0] = monitor;
    engine.num_fault_monitors = 1;
    
    // OK status
    set_signal(0, 100, LQ_EVENT_OK);
    lq_process_fault_monitors(&engine, 1000000);
    EXPECT_EQ(get_signal(1), 0);
    
    // Error status
    set_signal(0, 100, LQ_EVENT_ERROR);
    lq_process_fault_monitors(&engine, 1000000);
    EXPECT_EQ(get_signal(1), 3); // Level 3
    
    // Inconsistent status
    set_signal(0, 100, LQ_EVENT_INCONSISTENT);
    lq_process_fault_monitors(&engine, 1000000);
    EXPECT_EQ(get_signal(1), 3);
}

TEST_F(DriverTest, FaultMonitor_Staleness) {
    struct lq_fault_monitor_ctx monitor;
    monitor.input_signal = 0;
    monitor.fault_output_signal = 1;
    monitor.check_staleness = true;
    monitor.stale_timeout_us = 500000; // 500ms
    monitor.check_range = false;
    monitor.check_status = false;
    monitor.fault_level = LQ_FAULT_LEVEL_1;
    monitor.wake = nullptr;
    monitor.has_limp_action = false;
    monitor.enabled = true;
    
    engine.fault_monitors[0] = monitor;
    engine.num_fault_monitors = 1;
    
    // Fresh signal
    engine.signals[0].timestamp = 1000000;
    lq_process_fault_monitors(&engine, 1200000); // 200ms later
    EXPECT_EQ(get_signal(1), 0); // No fault
    
    // Stale signal
    lq_process_fault_monitors(&engine, 1600000); // 600ms later
    EXPECT_EQ(get_signal(1), 1); // Fault
}

TEST_F(DriverTest, FaultMonitor_Disabled) {
    struct lq_fault_monitor_ctx monitor;
    monitor.input_signal = 0;
    monitor.fault_output_signal = 1;
    monitor.check_staleness = false;
    monitor.check_range = true;
    monitor.min_value = 0;
    monitor.max_value = 100;
    monitor.check_status = false;
    monitor.fault_level = LQ_FAULT_LEVEL_1;
    monitor.wake = nullptr;
    monitor.has_limp_action = false;
    monitor.enabled = false; // Disabled
    
    engine.fault_monitors[0] = monitor;
    engine.num_fault_monitors = 1;
    
    set_signal(0, 200); // Out of range
    set_signal(1, 9999); // Pre-existing fault value
    
    lq_process_fault_monitors(&engine, 1000000);
    
    // Should not update
    EXPECT_EQ(get_signal(1), 9999);
}

/* ============================================================================
 * Integration Tests - Multiple Drivers
 * ============================================================================ */

TEST_F(DriverTest, Integration_RemapScalePID) {
    // Remap: joystick (±1000) -> throttle function
    struct lq_remap_ctx remap;
    remap.input_signal = 0;
    remap.output_signal = 1;
    remap.invert = false;
    remap.deadzone = 100;
    remap.enabled = true;
    
    // Scale: normalize to 0-100
    struct lq_scale_ctx scale;
    scale.input_signal = 1;
    scale.output_signal = 2;
    scale.scale_factor = 100; // 0.1x
    scale.offset = 50;        // Shift to 0-100 range
    scale.clamp_min = 0;
    scale.clamp_max = 100;
    scale.has_clamp_min = true;
    scale.has_clamp_max = true;
    scale.enabled = true;
    
    // PID: maintain speed setpoint
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 2;  // Scaled throttle command
    pid.measurement_signal = 3; // Actual speed
    pid.output_signal = 4;
    pid.kp = 5000;
    pid.ki = 100;
    pid.kd = 1000;
    pid.output_min = 0;
    pid.output_max = 100;
    pid.integral_min = -1000;
    pid.integral_max = 1000;
    pid.deadband = 2;
    pid.sample_time_us = 10000;
    pid.reset_on_setpoint_change = true;
    pid.first_run = true;
    pid.enabled = true;
    
    engine.remaps[0] = remap;
    engine.num_remaps = 1;
    engine.scales[0] = scale;
    engine.num_scales = 1;
    engine.pids[0] = pid;
    engine.num_pids = 1;
    
    // Joystick input
    set_signal(0, 500);  // Joystick
    set_signal(3, 40);   // Current speed
    
    // Process pipeline
    lq_process_remaps(&engine, engine.remaps, 1, 0);
    lq_process_scales(&engine, engine.scales, 1, 0);
    lq_process_pids(&engine, engine.pids, 1, 0);
    lq_process_pids(&engine, engine.pids, 1, 10000);
    
    // Verify remap (deadzone applied)
    EXPECT_EQ(get_signal(1), 500);
    
    // Verify scale
    EXPECT_EQ(get_signal(2), 100); // (500 * 0.1) + 50 = 100 (clamped)
    
    // Verify PID is running
    EXPECT_NE(get_signal(4), 0);
}

TEST_F(DriverTest, Integration_VerifiedOutputWithFaultMonitor) {
    // Verified output
    struct lq_verified_output_ctx vo;
    vo.command_signal = 0;
    vo.verification_signal = 1;
    vo.output_signal = 2;
    vo.output_type = LQ_VERIFIED_GPIO;
    vo.tolerance = 0;
    vo.verify_timeout_us = 0;
    vo.continuous_verify = true;
    vo.waiting_for_verify = false;
    vo.last_command = -1;
    vo.enabled = true;
    
    // Fault monitor watching verified output
    struct lq_fault_monitor_ctx monitor;
    monitor.input_signal = 2; // Watch verified output
    monitor.fault_output_signal = 3;
    monitor.check_staleness = false;
    monitor.check_range = false;
    monitor.check_status = true; // Check for FAULT status
    monitor.fault_level = LQ_FAULT_LEVEL_3;
    monitor.wake = nullptr;
    monitor.has_limp_action = false;
    monitor.enabled = true;
    
    engine.verified_outputs[0] = vo;
    engine.num_verified_outputs = 1;
    engine.fault_monitors[0] = monitor;
    engine.num_fault_monitors = 1;
    
    // Verification passes
    set_signal(0, 1);
    set_signal(1, 1);
    lq_process_verified_outputs(&engine, engine.verified_outputs, 1, 1000000);
    lq_process_fault_monitors(&engine, 1000000);
    
    EXPECT_EQ(get_status(2), LQ_EVENT_OK);
    EXPECT_EQ(get_signal(3), 0); // No fault
    
    // Verification fails
    set_signal(1, 0); // Mismatch
    lq_process_verified_outputs(&engine, engine.verified_outputs, 1, 1000000);
    lq_process_fault_monitors(&engine, 1000000);
    
    EXPECT_EQ(get_status(2), LQ_EVENT_ERROR);
    EXPECT_EQ(get_signal(3), 3); // Critical fault
}

/* ============================================================================
 * Edge Cases and Error Handling
 * ============================================================================ */

TEST_F(DriverTest, EdgeCase_InvalidSignalIndices) {
    struct lq_remap_ctx remap;
    remap.input_signal = 99; // Invalid
    remap.output_signal = 1;
    remap.invert = false;
    remap.deadzone = 0;
    remap.enabled = true;
    
    // Should not crash
    lq_process_remaps(&engine, &remap, 1, 1000000);
}

TEST_F(DriverTest, EdgeCase_ZeroSampleTime) {
    struct lq_pid_ctx pid;
    pid.setpoint_signal = 0;
    pid.measurement_signal = 1;
    pid.output_signal = 2;
    pid.kp = 1000;
    pid.ki = 1000;
    pid.kd = 1000;
    pid.output_min = -1000;
    pid.output_max = 1000;
    pid.integral_min = -1000000;
    pid.integral_max = 1000000;
    pid.deadband = 0;
    pid.sample_time_us = 0; // Variable time
    pid.reset_on_setpoint_change = true;
    pid.last_time = 1000000;
    pid.first_run = false;
    pid.enabled = true;
    
    set_signal(0, 100);
    set_signal(1, 90);
    
    // Same timestamp - dt=0, should not crash or divide by zero
    lq_process_pids(&engine, &pid, 1, 1000000);
}

TEST_F(DriverTest, EdgeCase_ExtremeValues) {
    struct lq_scale_ctx scale;
    scale.input_signal = 0;
    scale.output_signal = 1;
    scale.scale_factor = 1000;
    scale.offset = 0;
    scale.has_clamp_min = false;
    scale.has_clamp_max = false;
    scale.enabled = true;
    
    // INT32_MAX
    set_signal(0, INT32_MAX);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), INT32_MAX); // Should saturate, not overflow
    
    // INT32_MIN
    set_signal(0, INT32_MIN);
    lq_process_scales(&engine, &scale, 1, 1000000);
    EXPECT_EQ(get_signal(1), INT32_MIN);
}
