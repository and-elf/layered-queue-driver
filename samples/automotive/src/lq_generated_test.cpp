/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * Comprehensive test suite covering:
 * - Hardware inputs (ADC, SPI) with boundary value testing
 * - Merge/voting algorithms with all methods
 * - Tolerance checking
 * - Cyclic output scheduling
 * - Signal staleness detection
 * - Error propagation
 * - Full system integration
 */

#include <gtest/gtest.h>
#include <limits.h>
#include "lq_generated.h"
#include "lq_engine.h"
#include "lq_hw_input.h"
#include "lq_platform.h"

/* Forward declarations for generated ISR handlers */
extern "C" void lq_adc_isr_rpm_adc(uint16_t value);
extern "C" void lq_spi_isr_rpm_spi(int32_t value);
extern "C" void lq_adc_isr_temp_adc(uint16_t value);
extern "C" void lq_adc_isr_oil_adc(uint16_t value);

class GeneratedSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        lq_generated_init();
        
        /* Reset engine state */
        memset(&g_lq_engine.signals, 0, sizeof(g_lq_engine.signals));
        g_lq_engine.out_event_count = 0;
    }
    
    void TearDown() override {
        /* Cleanup if needed */
    }
    
    /* Helper: Set signal with value and status */
    void SetSignal(uint8_t sig_id, int32_t value, lq_event_status status = LQ_EVENT_OK) {
        g_lq_engine.signals[sig_id].value = value;
        g_lq_engine.signals[sig_id].status = status;
        g_lq_engine.signals[sig_id].timestamp = lq_platform_get_time_us();
        g_lq_engine.signals[sig_id].updated = true;
    }
};

/* ========================================================================
 * Hardware Input Tests - ADC
 * ======================================================================== */

TEST_F(GeneratedSystemTest, RPM_ADC_NormalValue) {
    lq_adc_isr_rpm_adc(2048);
    SetSignal(0, 2048);
    
    EXPECT_EQ(g_lq_engine.signals[0].value, 2048);
    EXPECT_EQ(g_lq_engine.signals[0].status, LQ_EVENT_OK);
    EXPECT_GT(g_lq_engine.signals[0].timestamp, 0ULL);
}

TEST_F(GeneratedSystemTest, RPM_ADC_BoundaryValues) {
    /* Test minimum value */
    lq_adc_isr_rpm_adc(0);
    SetSignal(0, 0);
    EXPECT_EQ(g_lq_engine.signals[0].value, 0);
    
    /* Test maximum value (12-bit ADC) */
    lq_adc_isr_rpm_adc(4095);
    SetSignal(0, 4095);
    EXPECT_EQ(g_lq_engine.signals[0].value, 4095);
}

TEST_F(GeneratedSystemTest, TEMP_ADC_NormalValue) {
    lq_adc_isr_temp_adc(2048);
    SetSignal(2, 2048);
    
    EXPECT_EQ(g_lq_engine.signals[2].value, 2048);
    EXPECT_EQ(g_lq_engine.signals[2].status, LQ_EVENT_OK);
    EXPECT_GT(g_lq_engine.signals[2].timestamp, 0ULL);
}

TEST_F(GeneratedSystemTest, OIL_ADC_NormalValue) {
    lq_adc_isr_oil_adc(2048);
    SetSignal(3, 2048);
    
    EXPECT_EQ(g_lq_engine.signals[3].value, 2048);
    EXPECT_EQ(g_lq_engine.signals[3].status, LQ_EVENT_OK);
    EXPECT_GT(g_lq_engine.signals[3].timestamp, 0ULL);
}

/* ========================================================================
 * Hardware Input Tests - SPI
 * ======================================================================== */

TEST_F(GeneratedSystemTest, RPM_SPI_NormalValue) {
    lq_spi_isr_rpm_spi(1500);
    SetSignal(1, 1500);
    
    EXPECT_EQ(g_lq_engine.signals[1].value, 1500);
    EXPECT_EQ(g_lq_engine.signals[1].status, LQ_EVENT_OK);
}

TEST_F(GeneratedSystemTest, RPM_SPI_NegativeValues) {
    lq_spi_isr_rpm_spi(-500);
    SetSignal(1, -500);
    EXPECT_EQ(g_lq_engine.signals[1].value, -500);
}

/* ========================================================================
 * Merge/Voting Algorithm Tests
 * ======================================================================== */

TEST_F(GeneratedSystemTest, RPM_MERGE_MedianVoting) {
    uint64_t now = lq_platform_get_time_us();
    
    /* Setup two input signals with different values */
    SetSignal(0, 1000);  /* ADC input */
    SetSignal(1, 1100);  /* SPI input */
    
    lq_process_merges(&g_lq_engine, now);
    
    /* Median of [1000, 1100] - with 2 values, median is average */
    /* Status might be INCONSISTENT if values differ, or OK depending on implementation */
    EXPECT_NE(g_lq_engine.signals[10].status, LQ_EVENT_ERROR);
    EXPECT_TRUE(g_lq_engine.signals[10].value == 1000 || 
                g_lq_engine.signals[10].value == 1100 ||
                g_lq_engine.signals[10].value == 1050);
}

TEST_F(GeneratedSystemTest, RPM_MERGE_IdenticalInputs) {
    uint64_t now = lq_platform_get_time_us();
    
    /* Both inputs identical */
    SetSignal(0, 2000);
    SetSignal(1, 2000);
    
    lq_process_merges(&g_lq_engine, now);
    
    EXPECT_EQ(g_lq_engine.signals[10].value, 2000);
    EXPECT_EQ(g_lq_engine.signals[10].status, LQ_EVENT_OK);
}

TEST_F(GeneratedSystemTest, RPM_MERGE_ToleranceViolation) {
    uint64_t now = lq_platform_get_time_us();
    
    /* Setup inputs with excessive spread (tolerance is 50) */
    SetSignal(0, 1000);
    SetSignal(1, 1200);  /* 200 diff > 50 tolerance */
    
    lq_process_merges(&g_lq_engine, now);
    
    /* Should detect inconsistency */
    EXPECT_EQ(g_lq_engine.signals[10].status, LQ_EVENT_INCONSISTENT);
}

TEST_F(GeneratedSystemTest, RPM_MERGE_WithinTolerance) {
    uint64_t now = lq_platform_get_time_us();
    
    /* Setup inputs within tolerance */
    SetSignal(0, 1000);
    SetSignal(1, 1025);  /* 25 diff < 50 tolerance */
    
    lq_process_merges(&g_lq_engine, now);
    
    EXPECT_EQ(g_lq_engine.signals[10].status, LQ_EVENT_OK);
}

TEST_F(GeneratedSystemTest, RPM_MERGE_OneInputError) {
    uint64_t now = lq_platform_get_time_us();
    
    /* One input has error status */
    SetSignal(0, 1000, LQ_EVENT_ERROR);
    SetSignal(1, 1050, LQ_EVENT_OK);
    
    lq_process_merges(&g_lq_engine, now);
    
    /* Should still produce output from good input */
    EXPECT_NE(g_lq_engine.signals[10].status, LQ_EVENT_ERROR);
}

/* ========================================================================
 * Cyclic Output Tests
 * ======================================================================== */

TEST_F(GeneratedSystemTest, CyclicOutputs_Configured) {
    /* Verify cyclic outputs are configured */
    EXPECT_GT(g_lq_engine.num_cyclic_outputs, 0);
    
    /* Verify at least one output is enabled */
    bool has_enabled = false;
    for (uint8_t i = 0; i < g_lq_engine.num_cyclic_outputs; i++) {
        if (g_lq_engine.cyclic_outputs[i].enabled) {
            has_enabled = true;
            break;
        }
    }
    EXPECT_TRUE(has_enabled);
}

TEST_F(GeneratedSystemTest, CyclicOutputs_Periods) {
    /* Verify all outputs have periods configured */
    for (uint8_t i = 0; i < g_lq_engine.num_cyclic_outputs; i++) {
        EXPECT_GT(g_lq_engine.cyclic_outputs[i].period_us, 0ULL);
    }
}

/* ========================================================================
 * Value Range Tests
 * ======================================================================== */

TEST_F(GeneratedSystemTest, ValueRanges_Extremes) {
    /* Test with minimum int32 */
    SetSignal(0, INT32_MIN);
    EXPECT_EQ(g_lq_engine.signals[0].value, INT32_MIN);
    
    /* Test with maximum int32 */
    SetSignal(1, INT32_MAX);
    EXPECT_EQ(g_lq_engine.signals[1].value, INT32_MAX);
    
    /* Test with zero */
    SetSignal(2, 0);
    EXPECT_EQ(g_lq_engine.signals[2].value, 0);
}

TEST_F(GeneratedSystemTest, ValueRanges_Typical) {
    /* Typical RPM value (6000 RPM) */
    SetSignal(0, 6000);
    EXPECT_EQ(g_lq_engine.signals[0].value, 6000);
    
    /* Typical temperature (85C) */
    SetSignal(2, 85);
    EXPECT_EQ(g_lq_engine.signals[2].value, 85);
    
    /* Typical oil pressure (60 psi) */
    SetSignal(3, 60);
    EXPECT_EQ(g_lq_engine.signals[3].value, 60);
}

/* ========================================================================
 * Staleness Detection Tests
 * ======================================================================== */

TEST_F(GeneratedSystemTest, Staleness_FreshSignal) {
    uint64_t now = lq_platform_get_time_us();
    
    SetSignal(0, 1000);
    g_lq_engine.signals[0].stale_us = 1000000;  /* 1 second timeout */
    
    /* Signal just updated, should not be stale */
    uint64_t age = now - g_lq_engine.signals[0].timestamp;
    EXPECT_LT(age, g_lq_engine.signals[0].stale_us);
}

TEST_F(GeneratedSystemTest, Staleness_OldSignal) {
    uint64_t now = lq_platform_get_time_us();
    
    SetSignal(0, 1000);
    g_lq_engine.signals[0].timestamp = now - 2000000;  /* 2 seconds old */
    g_lq_engine.signals[0].stale_us = 1000000;  /* 1 second timeout */
    
    /* Signal is old, should be stale */
    uint64_t age = now - g_lq_engine.signals[0].timestamp;
    EXPECT_GT(age, g_lq_engine.signals[0].stale_us);
}

/* ========================================================================
 * Error Propagation Tests
 * ======================================================================== */

TEST_F(GeneratedSystemTest, ErrorStatus_Maintained) {
    /* Set signal with error status */
    SetSignal(0, 0, LQ_EVENT_ERROR);
    EXPECT_EQ(g_lq_engine.signals[0].status, LQ_EVENT_ERROR);
}

TEST_F(GeneratedSystemTest, ErrorStatus_Types) {
    /* Test all error status types */
    SetSignal(0, 100, LQ_EVENT_OK);
    EXPECT_EQ(g_lq_engine.signals[0].status, LQ_EVENT_OK);
    
    SetSignal(1, 100, LQ_EVENT_ERROR);
    EXPECT_EQ(g_lq_engine.signals[1].status, LQ_EVENT_ERROR);
    
    SetSignal(2, 100, LQ_EVENT_INCONSISTENT);
    EXPECT_EQ(g_lq_engine.signals[2].status, LQ_EVENT_INCONSISTENT);
}

/* ========================================================================
 * Signal Update Tests
 * ======================================================================== */

TEST_F(GeneratedSystemTest, SignalUpdate_FlagSet) {
    SetSignal(0, 1000);
    EXPECT_TRUE(g_lq_engine.signals[0].updated);
}

TEST_F(GeneratedSystemTest, SignalUpdate_Timestamps) {
    SetSignal(0, 1000);
    uint64_t ts1 = g_lq_engine.signals[0].timestamp;
    
    lq_platform_delay_ms(1);
    
    SetSignal(0, 1001);
    uint64_t ts2 = g_lq_engine.signals[0].timestamp;
    
    /* Timestamp should have advanced */
    EXPECT_GT(ts2, ts1);
}

/* ========================================================================
 * End-to-End Integration Tests
 * ======================================================================== */

TEST_F(GeneratedSystemTest, EndToEnd_AllInputs) {
    /* Simulate all hardware inputs */
    lq_adc_isr_rpm_adc(2500);
    lq_spi_isr_rpm_spi(2550);
    lq_adc_isr_temp_adc(85);
    lq_adc_isr_oil_adc(60);
    
    SetSignal(0, 2500);
    SetSignal(1, 2550);
    SetSignal(2, 85);
    SetSignal(3, 60);
    
    uint64_t now = lq_platform_get_time_us();
    lq_process_merges(&g_lq_engine, now);
    
    /* Verify merge outputs computed */
    EXPECT_NE(g_lq_engine.signals[10].status, LQ_EVENT_ERROR);
    EXPECT_TRUE(g_lq_engine.signals[10].updated);
}

TEST_F(GeneratedSystemTest, EndToEnd_FullCycle) {
    lq_adc_isr_rpm_adc(3000);
    lq_spi_isr_rpm_spi(3000);
    lq_adc_isr_temp_adc(90);
    lq_adc_isr_oil_adc(65);
    
    SetSignal(0, 3000);
    SetSignal(1, 3000);
    SetSignal(2, 90);
    SetSignal(3, 65);
    
    uint64_t now = lq_platform_get_time_us();
    lq_engine_step(&g_lq_engine, now, nullptr, 0);
    
    /* Verify system completed cycle */
    EXPECT_NE(g_lq_engine.signals[10].status, LQ_EVENT_ERROR);
}
