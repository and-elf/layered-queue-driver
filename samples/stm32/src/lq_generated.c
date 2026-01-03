/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 */

#include "lq_generated.h"
#include "lq_hw_input.h"
#include "lq_common.h"
#include "lq_event.h"
#include <string.h>

/* Merge contexts */
static struct lq_merge_ctx g_merges[1] = {
    [0] = {
        .output_signal = 20,
        .input_signals = {0, 1, 10},
        .num_inputs = 3,
        .voting_method = LQ_VOTE_MEDIAN,
        .tolerance = 50,
        .stale_us = 150000,
    },
};

/* Cyclic output contexts */
static struct lq_cyclic_ctx g_cyclic_outputs[4] = {
    [0] = {
        .signal_id = 20,
        .output_type = LQ_OUTPUT_J1939,
        .target_id = 65265,
        .period_us = 100000,
        .next_deadline_us = 0,
        .priority = 3,
    },
    [1] = {
        .signal_id = 3,
        .output_type = LQ_OUTPUT_J1939,
        .target_id = 65262,
        .period_us = 1000000,
        .next_deadline_us = 0,
        .priority = 6,
    },
    [2] = {
        .signal_id = 2,
        .output_type = LQ_OUTPUT_J1939,
        .target_id = 65263,
        .period_us = 1000000,
        .next_deadline_us = 0,
        .priority = 6,
    },
    [3] = {
        .signal_id = 12,
        .output_type = LQ_OUTPUT_J1939,
        .target_id = 65266,
        .period_us = 100000,
        .next_deadline_us = 0,
        .priority = 3,
    },
};

/* Engine instance */
struct lq_engine g_lq_engine = {
    .num_signals = 64,
    .num_merges = 1,
    .num_cyclic_outputs = 4,
    .signals = {0},
    .merges = g_merges,
    .cyclic_outputs = g_cyclic_outputs,
};

/* ADC ISR for rpm_adc_1 */
void lq_adc_isr_rpm_adc_1(uint16_t value) {
    lq_hw_push(0, (int32_t)value, lq_platform_get_time_us());
}

/* ADC ISR for rpm_adc_2 */
void lq_adc_isr_rpm_adc_2(uint16_t value) {
    lq_hw_push(1, (int32_t)value, lq_platform_get_time_us());
}

/* ADC ISR for oil_pressure_adc */
void lq_adc_isr_oil_pressure_adc(uint16_t value) {
    lq_hw_push(2, (int32_t)value, lq_platform_get_time_us());
}

/* ADC ISR for coolant_temp_adc */
void lq_adc_isr_coolant_temp_adc(uint16_t value) {
    lq_hw_push(3, (int32_t)value, lq_platform_get_time_us());
}

/* Initialization */
int lq_generated_init(void) {
    /* Hardware input layer */
    int ret = lq_hw_input_init(64);
    if (ret != 0) return ret;
    
    /* Platform-specific peripheral init */
    #ifdef LQ_PLATFORM_INIT
    lq_platform_peripherals_init();
    #endif
    
    return 0;
}
