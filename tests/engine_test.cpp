/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Google Test tests for Engine Core
 * Comprehensive coverage for production validation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "lq_engine.h"
#include "lq_event.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class EngineTest : public ::testing::Test {
protected:
    struct lq_engine engine;
    
    // Track wake function calls
    static std::vector<uint8_t> wake_calls;
    static std::vector<int32_t> wake_values;
    static std::vector<enum lq_fault_level> wake_levels;
    
    void SetUp() override {
        memset(&engine, 0, sizeof(engine));
        engine.num_signals = LQ_MAX_SIGNALS;
        lq_engine_init(&engine);
        
        // Clear wake tracking
        wake_calls.clear();
        wake_values.clear();
        wake_levels.clear();
    }
    
    void TearDown() override {
        // Cleanup if needed
    }
    
    // Helper: Create event
    struct lq_event make_event(uint8_t source_id, int32_t value, 
                                 enum lq_event_status status = LQ_EVENT_OK,
                                 uint64_t timestamp = 1000000) {
        struct lq_event evt;
        evt.source_id = source_id;
        evt.value = value;
        evt.status = status;
        evt.timestamp = timestamp;
        return evt;
    }
    
    // Static wake callback for testing
    static void test_wake_callback(uint8_t monitor_id, int32_t value, enum lq_fault_level level) {
        wake_calls.push_back(monitor_id);
        wake_values.push_back(value);
        wake_levels.push_back(level);
    }
};

// Static member initialization
std::vector<uint8_t> EngineTest::wake_calls;
std::vector<int32_t> EngineTest::wake_values;
std::vector<enum lq_fault_level> EngineTest::wake_levels;

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(EngineTest, InitBasic) {
    EXPECT_EQ(engine.out_event_count, 0);
    EXPECT_EQ(engine.num_signals, LQ_MAX_SIGNALS);
    
    // All signals should be initialized
    for (int i = 0; i < LQ_MAX_SIGNALS; i++) {
        EXPECT_FALSE(engine.signals[i].updated);
    }
}

TEST_F(EngineTest, InitNullEngine) {
    int ret = lq_engine_init(nullptr);
    EXPECT_EQ(ret, -22);  // -EINVAL
}

// ============================================================================
// Event Ingestion Tests
// ============================================================================

TEST_F(EngineTest, IngestSingleEvent) {
    struct lq_event evt = make_event(5, 1234);
    
    lq_ingest_events(&engine, &evt, 1);
    
    EXPECT_EQ(engine.signals[5].value, 1234);
    EXPECT_EQ(engine.signals[5].status, LQ_EVENT_OK);
    EXPECT_EQ(engine.signals[5].timestamp, 1000000);
    EXPECT_TRUE(engine.signals[5].updated);
}

TEST_F(EngineTest, IngestMultipleEvents) {
    struct lq_event events[3];
    events[0] = make_event(0, 100);
    events[1] = make_event(1, 200);
    events[2] = make_event(2, 300);
    
    lq_ingest_events(&engine, events, 3);
    
    EXPECT_EQ(engine.signals[0].value, 100);
    EXPECT_EQ(engine.signals[1].value, 200);
    EXPECT_EQ(engine.signals[2].value, 300);
}

TEST_F(EngineTest, IngestUpdateExistingSignal) {
    struct lq_event evt1 = make_event(5, 100);
    lq_ingest_events(&engine, &evt1, 1);
    
    EXPECT_TRUE(engine.signals[5].updated);
    
    // Update with same value - should not be marked as updated
    engine.signals[5].updated = false;
    struct lq_event evt2 = make_event(5, 100);
    lq_ingest_events(&engine, &evt2, 1);
    EXPECT_FALSE(engine.signals[5].updated);
    
    // Update with different value - should be marked as updated
    struct lq_event evt3 = make_event(5, 200);
    lq_ingest_events(&engine, &evt3, 1);
    EXPECT_TRUE(engine.signals[5].updated);
}

TEST_F(EngineTest, IngestInvalidSignalId) {
    struct lq_event evt = make_event(LQ_MAX_SIGNALS + 1, 1234);
    
    lq_ingest_events(&engine, &evt, 1);
    
    // Should be ignored - no crash
}

TEST_F(EngineTest, IngestWithErrorStatus) {
    struct lq_event evt = make_event(3, 500, LQ_EVENT_ERROR);
    
    lq_ingest_events(&engine, &evt, 1);
    
    EXPECT_EQ(engine.signals[3].value, 500);
    EXPECT_EQ(engine.signals[3].status, LQ_EVENT_ERROR);
}

// ============================================================================
// Merge/Voting Tests
// ============================================================================

TEST_F(EngineTest, MergeMedianVoting) {
    // Configure merge: inputs 0,1,2 -> output 10
    engine.num_merges = 1;
    engine.merges[0].input_signals[0] = 0;
    engine.merges[0].input_signals[1] = 1;
    engine.merges[0].input_signals[2] = 2;
    engine.merges[0].num_inputs = 3;
    engine.merges[0].output_signal = 10;
    engine.merges[0].voting_method = LQ_VOTE_MEDIAN;
    engine.merges[0].enabled = true;
    
    // Set input values: 100, 200, 150
    struct lq_event events[3];
    events[0] = make_event(0, 100);
    events[1] = make_event(1, 200);
    events[2] = make_event(2, 150);
    
    lq_ingest_events(&engine, events, 3);
    lq_process_merges(&engine, 1000000);
    
    // Median of [100, 150, 200] is 150
    EXPECT_EQ(engine.signals[10].value, 150);
    EXPECT_EQ(engine.signals[10].status, LQ_EVENT_OK);
}

TEST_F(EngineTest, MergeAverageVoting) {
    engine.num_merges = 1;
    engine.merges[0].input_signals[0] = 0;
    engine.merges[0].input_signals[1] = 1;
    engine.merges[0].num_inputs = 2;
    engine.merges[0].output_signal = 10;
    engine.merges[0].voting_method = LQ_VOTE_AVERAGE;
    engine.merges[0].enabled = true;
    
    struct lq_event events[2];
    events[0] = make_event(0, 100);
    events[1] = make_event(1, 300);
    
    lq_ingest_events(&engine, events, 2);
    lq_process_merges(&engine, 1000000);
    
    // Average of [100, 300] is 200
    EXPECT_EQ(engine.signals[10].value, 200);
}

TEST_F(EngineTest, MergeToleranceCheck) {
    engine.num_merges = 1;
    engine.merges[0].input_signals[0] = 0;
    engine.merges[0].input_signals[1] = 1;
    engine.merges[0].num_inputs = 2;
    engine.merges[0].output_signal = 10;
    engine.merges[0].voting_method = LQ_VOTE_MEDIAN;
    engine.merges[0].tolerance = 50;  // Max deviation 50
    engine.merges[0].enabled = true;
    
    // Within tolerance: 100 and 120 (diff = 20)
    struct lq_event events1[2];
    events1[0] = make_event(0, 100);
    events1[1] = make_event(1, 120);
    
    lq_ingest_events(&engine, events1, 2);
    lq_process_merges(&engine, 1000000);
    
    EXPECT_EQ(engine.signals[10].status, LQ_EVENT_OK);
    
    // Outside tolerance: 100 and 200 (diff = 100)
    struct lq_event events2[2];
    events2[0] = make_event(0, 100);
    events2[1] = make_event(1, 200);
    
    lq_ingest_events(&engine, events2, 2);
    lq_process_merges(&engine, 1000000);
    
    EXPECT_EQ(engine.signals[10].status, LQ_EVENT_INCONSISTENT);
}

TEST_F(EngineTest, MergeDisabled) {
    engine.num_merges = 1;
    engine.merges[0].input_signals[0] = 0;
    engine.merges[0].input_signals[1] = 1;
    engine.merges[0].num_inputs = 2;
    engine.merges[0].output_signal = 10;
    engine.merges[0].voting_method = LQ_VOTE_MEDIAN;
    engine.merges[0].enabled = false;  // Disabled
    
    struct lq_event events[2];
    events[0] = make_event(0, 100);
    events[1] = make_event(1, 200);
    
    lq_ingest_events(&engine, events, 2);
    
    int old_value = engine.signals[10].value;
    lq_process_merges(&engine, 1000000);
    
    // Should not update when disabled
    EXPECT_EQ(engine.signals[10].value, old_value);
}

// ============================================================================
// Fault Monitor Tests
// ============================================================================

TEST_F(EngineTest, FaultMonitorRangeCheckWakeImmediate) {
    // Configure fault monitor with wake function
    engine.num_fault_monitors = 1;
    engine.fault_monitors[0].input_signal = 5;
    engine.fault_monitors[0].fault_output_signal = 20;
    engine.fault_monitors[0].check_range = true;
    engine.fault_monitors[0].min_value = 0;
    engine.fault_monitors[0].max_value = 1000;
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_2;
    engine.fault_monitors[0].wake = test_wake_callback;
    engine.fault_monitors[0].enabled = true;
    
    // Ingest out-of-range value - should trigger wake immediately
    struct lq_event evt = make_event(5, 1500);  // Above max
    lq_ingest_events(&engine, &evt, 1);
    
    // Wake should have been called immediately during ingestion
    EXPECT_EQ(wake_calls.size(), 1);
    EXPECT_EQ(wake_calls[0], 0);  // Monitor ID 0
    EXPECT_EQ(wake_values[0], 1500);
    EXPECT_EQ(wake_levels[0], LQ_FAULT_LEVEL_2);
}

TEST_F(EngineTest, FaultMonitorRangeCheckNoWakeInRange) {
    engine.num_fault_monitors = 1;
    engine.fault_monitors[0].input_signal = 5;
    engine.fault_monitors[0].fault_output_signal = 20;
    engine.fault_monitors[0].check_range = true;
    engine.fault_monitors[0].min_value = 0;
    engine.fault_monitors[0].max_value = 1000;
    engine.fault_monitors[0].wake = test_wake_callback;
    engine.fault_monitors[0].enabled = true;
    
    // In-range value - no wake
    struct lq_event evt = make_event(5, 500);
    lq_ingest_events(&engine, &evt, 1);
    
    EXPECT_EQ(wake_calls.size(), 0);
}

TEST_F(EngineTest, FaultMonitorFullProcessing) {
    engine.num_fault_monitors = 1;
    engine.fault_monitors[0].input_signal = 5;
    engine.fault_monitors[0].fault_output_signal = 20;
    engine.fault_monitors[0].check_range = true;
    engine.fault_monitors[0].min_value = 0;
    engine.fault_monitors[0].max_value = 1000;
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_3;
    engine.fault_monitors[0].enabled = true;
    
    // Out of range
    struct lq_event evt = make_event(5, 1500);
    lq_ingest_events(&engine, &evt, 1);
    lq_process_fault_monitors(&engine, 1000000);
    
    // Fault output should be set to fault level
    EXPECT_EQ(engine.signals[20].value, (int32_t)LQ_FAULT_LEVEL_3);
    EXPECT_EQ(engine.signals[20].status, LQ_EVENT_OK);
}

TEST_F(EngineTest, FaultMonitorStalenessDetection) {
    engine.num_fault_monitors = 1;
    engine.fault_monitors[0].input_signal = 5;
    engine.fault_monitors[0].fault_output_signal = 20;
    engine.fault_monitors[0].check_staleness = true;
    engine.fault_monitors[0].stale_timeout_us = 1000000;  // 1 second
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_1;
    engine.fault_monitors[0].enabled = true;
    
    // Set initial value at t=1000000
    struct lq_event evt = make_event(5, 100, LQ_EVENT_OK, 1000000);
    lq_ingest_events(&engine, &evt, 1);
    
    // Check at t=1500000 (500ms later) - not stale
    lq_process_fault_monitors(&engine, 1500000);
    EXPECT_EQ(engine.signals[20].value, 0);  // No fault
    
    // Check at t=2500000 (1.5s later) - stale
    lq_process_fault_monitors(&engine, 2500000);
    EXPECT_EQ(engine.signals[20].value, (int32_t)LQ_FAULT_LEVEL_1);
}

TEST_F(EngineTest, FaultMonitorStatusCheck) {
    engine.num_fault_monitors = 1;
    engine.fault_monitors[0].input_signal = 5;
    engine.fault_monitors[0].fault_output_signal = 20;
    engine.fault_monitors[0].check_status = true;
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_2;
    engine.fault_monitors[0].enabled = true;
    
    // Event with error status
    struct lq_event evt = make_event(5, 100, LQ_EVENT_ERROR);
    lq_ingest_events(&engine, &evt, 1);
    lq_process_fault_monitors(&engine, 1000000);
    
    EXPECT_EQ(engine.signals[20].value, (int32_t)LQ_FAULT_LEVEL_2);
}

// ============================================================================
// Dual Inverted Input with GPIO Error Output Test
// (User-requested specific scenario)
// ============================================================================

TEST_F(EngineTest, DualInvertedInputWithIncorrectValueGpioError) {
    /*
     * Scenario: Two redundant inverted inputs with voting
     * - Input 0: Primary sensor (inverted)
     * - Input 1: Secondary sensor (inverted)
     * - Merge output: Signal 10 (voted/median)
     * - Fault monitor watches merge output
     * - GPIO error output: Signal 20
     * 
     * Test case: One input has incorrect value, triggering fault
     */
    
    // Configure two remaps for inverted inputs
    engine.num_remaps = 2;
    
    // Remap 0: Signal 0 -> Signal 2 (inverted)
    engine.remaps[0].input_signal = 0;
    engine.remaps[0].output_signal = 2;
    engine.remaps[0].invert = true;
    engine.remaps[0].deadzone = 0;
    engine.remaps[0].enabled = true;
    
    // Remap 1: Signal 1 -> Signal 3 (inverted)
    engine.remaps[1].input_signal = 1;
    engine.remaps[1].output_signal = 3;
    engine.remaps[1].invert = true;
    engine.remaps[1].deadzone = 0;
    engine.remaps[1].enabled = true;
    
    // Configure merge with tolerance check
    engine.num_merges = 1;
    engine.merges[0].input_signals[0] = 2;  // Inverted input 0
    engine.merges[0].input_signals[1] = 3;  // Inverted input 1
    engine.merges[0].num_inputs = 2;
    engine.merges[0].output_signal = 10;
    engine.merges[0].voting_method = LQ_VOTE_MEDIAN;
    engine.merges[0].tolerance = 50;  // Max 50 difference allowed
    engine.merges[0].enabled = true;
    
    // Configure fault monitor on merge output
    engine.num_fault_monitors = 1;
    engine.fault_monitors[0].input_signal = 10;
    engine.fault_monitors[0].fault_output_signal = 20;  // GPIO error output
    engine.fault_monitors[0].check_status = true;  // Check for INCONSISTENT
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_3;
    engine.fault_monitors[0].wake = test_wake_callback;
    engine.fault_monitors[0].enabled = true;
    
    // Test 1: Both inputs agree (within tolerance)
    struct lq_event events_ok[2];
    events_ok[0] = make_event(0, 500);
    events_ok[1] = make_event(1, 520);  // Close to first value
    
    lq_ingest_events(&engine, events_ok, 2);
    lq_process_remaps(&engine, engine.remaps, engine.num_remaps, 1000000);
    lq_process_merges(&engine, 1000000);
    lq_process_fault_monitors(&engine, 1000000);
    
    // After inversion: -500 and -520
    EXPECT_EQ(engine.signals[2].value, -500);
    EXPECT_EQ(engine.signals[3].value, -520);
    // Median should work
    EXPECT_EQ(engine.signals[10].status, LQ_EVENT_OK);
    // GPIO error should be clear
    EXPECT_EQ(engine.signals[20].value, 0);
    
    // Test 2: One input has incorrect value (outside tolerance)
    struct lq_event events_fault[2];
    events_fault[0] = make_event(0, 500);
    events_fault[1] = make_event(1, 700);  // Significantly different
    
    lq_ingest_events(&engine, events_fault, 2);
    lq_process_remaps(&engine, engine.remaps, engine.num_remaps, 1000000);
    lq_process_merges(&engine, 1000000);
    lq_process_fault_monitors(&engine, 1000000);
    
    // After inversion: -500 and -700 (diff = 200, exceeds tolerance of 50)
    EXPECT_EQ(engine.signals[2].value, -500);
    EXPECT_EQ(engine.signals[3].value, -700);
    // Merge should detect inconsistency
    EXPECT_EQ(engine.signals[10].status, LQ_EVENT_INCONSISTENT);
    // GPIO error should be set to fault level
    EXPECT_EQ(engine.signals[20].value, (int32_t)LQ_FAULT_LEVEL_3);
    EXPECT_TRUE(engine.signals[20].updated);
}

// ============================================================================
// Staleness Detection Tests
// ============================================================================

TEST_F(EngineTest, StalenessDetection) {
    engine.signals[5].stale_us = 1000000;  // 1 second timeout
    
    // Set initial value at t=1000000
    struct lq_event evt = make_event(5, 100, LQ_EVENT_OK, 1000000);
    lq_ingest_events(&engine, &evt, 1);
    
    // Check at t=1500000 (500ms) - not stale
    lq_apply_input_staleness(&engine, 1500000);
    EXPECT_EQ(engine.signals[5].status, LQ_EVENT_OK);
    
    // Check at t=2500000 (1.5s) - stale
    lq_apply_input_staleness(&engine, 2500000);
    EXPECT_EQ(engine.signals[5].status, LQ_EVENT_TIMEOUT);
}

TEST_F(EngineTest, StalenessNoCheckWhenZero) {
    engine.signals[5].stale_us = 0;  // No staleness check
    
    struct lq_event evt = make_event(5, 100, LQ_EVENT_OK, 1000000);
    lq_ingest_events(&engine, &evt, 1);
    
    // Even after long time, should not be marked stale
    lq_apply_input_staleness(&engine, 10000000);
    EXPECT_EQ(engine.signals[5].status, LQ_EVENT_OK);
}

// ============================================================================
// Cyclic Output Tests
// ============================================================================

TEST_F(EngineTest, CyclicOutputScheduling) {
    // Configure cyclic output: Signal 5 every 100ms
    engine.num_cyclic_outputs = 1;
    engine.cyclic_outputs[0].source_signal = 5;
    engine.cyclic_outputs[0].period_us = 100000;  // 100ms
    engine.cyclic_outputs[0].next_deadline = 0;
    engine.cyclic_outputs[0].type = LQ_OUTPUT_CAN;
    engine.cyclic_outputs[0].target_id = 0x123;
    engine.cyclic_outputs[0].enabled = true;
    
    // Set signal value
    engine.signals[5].value = 1234;
    engine.signals[5].status = LQ_EVENT_OK;
    
    // Process at t=0 - should output
    lq_process_cyclic_outputs(&engine, 0);
    EXPECT_GT(engine.out_event_count, 0);
    EXPECT_EQ(engine.out_events[0].value, 1234);
    EXPECT_EQ(engine.out_events[0].target_id, 0x123);
    
    // Next deadline should be set to 100ms
    EXPECT_EQ(engine.cyclic_outputs[0].next_deadline, 100000);
    
    // Process at t=50ms - should not output
    engine.out_event_count = 0;
    lq_process_cyclic_outputs(&engine, 50000);
    EXPECT_EQ(engine.out_event_count, 0);
    
    // Process at t=100ms - should output again
    lq_process_cyclic_outputs(&engine, 100000);
    EXPECT_GT(engine.out_event_count, 0);
}

TEST_F(EngineTest, CyclicOutputDisabled) {
    engine.num_cyclic_outputs = 1;
    engine.cyclic_outputs[0].source_signal = 5;
    engine.cyclic_outputs[0].period_us = 100000;
    engine.cyclic_outputs[0].next_deadline = 0;
    engine.cyclic_outputs[0].enabled = false;  // Disabled
    
    engine.signals[5].value = 1234;
    
    lq_process_cyclic_outputs(&engine, 0);
    EXPECT_EQ(engine.out_event_count, 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(EngineTest, FullPipelineRemapMergeFault) {
    // Complete pipeline: Raw inputs -> Remap -> Merge -> Fault Monitor
    
    // Configure remaps (invert two inputs)
    engine.num_remaps = 2;
    engine.remaps[0].input_signal = 0;
    engine.remaps[0].output_signal = 10;
    engine.remaps[0].invert = true;
    engine.remaps[0].enabled = true;
    
    engine.remaps[1].input_signal = 1;
    engine.remaps[1].output_signal = 11;
    engine.remaps[1].invert = true;
    engine.remaps[1].enabled = true;
    
    // Configure merge
    engine.num_merges = 1;
    engine.merges[0].input_signals[0] = 10;
    engine.merges[0].input_signals[1] = 11;
    engine.merges[0].num_inputs = 2;
    engine.merges[0].output_signal = 20;
    engine.merges[0].voting_method = LQ_VOTE_MEDIAN;
    engine.merges[0].tolerance = 30;
    engine.merges[0].enabled = true;
    
    // Configure fault monitor
    engine.num_fault_monitors = 1;
    engine.fault_monitors[0].input_signal = 20;
    engine.fault_monitors[0].fault_output_signal = 30;
    engine.fault_monitors[0].check_status = true;
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_2;
    engine.fault_monitors[0].enabled = true;
    
    // Input events
    struct lq_event events[2];
    events[0] = make_event(0, 100);
    events[1] = make_event(1, 110);
    
    // Process through pipeline
    lq_ingest_events(&engine, events, 2);
    lq_process_remaps(&engine, engine.remaps, engine.num_remaps, 1000000);
    lq_process_merges(&engine, 1000000);
    lq_process_fault_monitors(&engine, 1000000);
    
    // Verify pipeline
    EXPECT_EQ(engine.signals[10].value, -100);  // Inverted
    EXPECT_EQ(engine.signals[11].value, -110);  // Inverted
    EXPECT_EQ(engine.signals[20].status, LQ_EVENT_OK);  // Within tolerance
    EXPECT_EQ(engine.signals[30].value, 0);  // No fault
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(EngineTest, MaxSignalsHandling) {
    // Test with maximum signal count
    struct lq_event events[LQ_MAX_SIGNALS];
    for (int i = 0; i < LQ_MAX_SIGNALS; i++) {
        events[i] = make_event((uint8_t)i, i * 10);
    }
    
    lq_ingest_events(&engine, events, LQ_MAX_SIGNALS);
    
    for (int i = 0; i < LQ_MAX_SIGNALS; i++) {
        EXPECT_EQ(engine.signals[i].value, i * 10);
    }
}

TEST_F(EngineTest, ZeroEventsProcessing) {
    lq_ingest_events(&engine, nullptr, 0);
    // Should not crash
}

TEST_F(EngineTest, MultipleFaultMonitorsOnSameSignal) {
    // Multiple monitors can watch the same signal
    engine.num_fault_monitors = 2;
    
    engine.fault_monitors[0].input_signal = 5;
    engine.fault_monitors[0].fault_output_signal = 20;
    engine.fault_monitors[0].check_range = true;
    engine.fault_monitors[0].min_value = 0;
    engine.fault_monitors[0].max_value = 100;
    engine.fault_monitors[0].fault_level = LQ_FAULT_LEVEL_1;
    engine.fault_monitors[0].enabled = true;
    
    engine.fault_monitors[1].input_signal = 5;
    engine.fault_monitors[1].fault_output_signal = 21;
    engine.fault_monitors[1].check_range = true;
    engine.fault_monitors[1].min_value = -50;
    engine.fault_monitors[1].max_value = 50;
    engine.fault_monitors[1].fault_level = LQ_FAULT_LEVEL_2;
    engine.fault_monitors[1].enabled = true;
    
    struct lq_event evt = make_event(5, 75);
    lq_ingest_events(&engine, &evt, 1);
    lq_process_fault_monitors(&engine, 1000000);
    
    // First monitor: OK (0-100 range)
    EXPECT_EQ(engine.signals[20].value, 0);
    
    // Second monitor: FAULT (exceeds -50 to 50 range)
    EXPECT_EQ(engine.signals[21].value, (int32_t)LQ_FAULT_LEVEL_2);
}
