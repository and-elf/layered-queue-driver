/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 */

#include "lq_generated.h"
#include "lq_hw_input.h"
#include "lq_common.h"
#include "lq_event.h"
#include "lq_hil.h"
#include <string.h>

/* Merge contexts */
static struct lq_merge_ctx g_merges[1] = {
    [0] = {
        .output_signal = 10,
        .input_signals = {0, 1},
        .num_inputs = 2,
        .voting_method = LQ_VOTE_MEDIAN,
        .tolerance = 50,
        .stale_us = 10000,
    },
};

/* Cyclic output contexts */
static struct lq_cyclic_ctx g_cyclic_outputs[3] = {
    [0] = {
        .type = LQ_OUTPUT_J1939,
        .target_id = 65265,
        .source_signal = 10,
        .period_us = 100000,
        .next_deadline = 0,
        .flags = 0,
        .enabled = true,
    },
    [1] = {
        .type = LQ_OUTPUT_J1939,
        .target_id = 65262,
        .source_signal = 2,
        .period_us = 1000000,
        .next_deadline = 10000,
        .flags = 0,
        .enabled = true,
    },
    [2] = {
        .type = LQ_OUTPUT_J1939,
        .target_id = 65263,
        .source_signal = 3,
        .period_us = 200000,
        .next_deadline = 20000,
        .flags = 0,
        .enabled = true,
    },
};

/* Engine instance */
struct lq_engine g_lq_engine = {
    .num_signals = 0,  /* Initialized at runtime */
    .num_merges = 1,
    .num_cyclic_outputs = 3,
};

/* ADC ISR for rpm_adc */
void lq_adc_isr_rpm_adc(uint16_t value) {
    lq_hw_push(0, (uint32_t)value);
}

/* SPI ISR for rpm_spi */
void lq_spi_isr_rpm_spi(int32_t value) {
    lq_hw_push(1, (uint32_t)value);
}

/* ADC ISR for temp_adc */
void lq_adc_isr_temp_adc(uint16_t value) {
    lq_hw_push(2, (uint32_t)value);
}

/* ADC ISR for oil_adc */
void lq_adc_isr_oil_adc(uint16_t value) {
    lq_hw_push(3, (uint32_t)value);
}

/* Initialization */
int lq_generated_init(void) {
    /* Auto-detect HIL mode on native platform */
    #ifdef LQ_PLATFORM_NATIVE
    lq_hil_init(LQ_HIL_MODE_DISABLED, 0);  /* Auto-detects from env */
    #endif
    
    /* Hardware input layer */
    int ret = lq_hw_input_init(64);
    if (ret != 0) return ret;
    
    /* Copy merge contexts to engine */
    memcpy(g_lq_engine.merges, g_merges, sizeof(g_merges));
    
    /* Copy cyclic output contexts to engine */
    memcpy(g_lq_engine.cyclic_outputs, g_cyclic_outputs, sizeof(g_cyclic_outputs));
    
    /* Platform-specific peripheral init */
    #ifdef LQ_PLATFORM_INIT
    lq_platform_peripherals_init();
    #endif
    
    return 0;
}
