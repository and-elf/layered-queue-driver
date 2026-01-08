/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 */

#include "lq_generated.h"
#include "lq_hw_input.h"
#include "lq_common.h"
#include "lq_event.h"
#include "lq_hil.h"
#include "lq_j1939.h"
#include "lq_platform.h"  /* For lq_can_send */
#include <stdlib.h>
#include <string.h>

/* Platform function declarations - implement these in your platform code
 * or link with lq_platform_stubs.c for default no-op implementations */
extern int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len);

/* Engine instance */
struct lq_engine g_lq_engine = {
    .num_signals = 11,
    .num_merges = 1,
    .num_fault_monitors = 0,
    .num_cyclic_outputs = 3,
    .merges = {
        [0] = {
            .output_signal = 10,
            .input_signals = {0, 1},
            .num_inputs = 2,
            .voting_method = LQ_VOTE_MEDIAN,
            .tolerance = 50,
            .stale_us = 10000,
            .enabled = true,
        },
    },
    .cyclic_outputs = {
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
    },
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
    /* Auto-detect HIL mode on native platform (if not already initialized) */
    #ifdef LQ_PLATFORM_NATIVE
    if (!lq_hil_is_active()) {
        lq_hil_init(LQ_HIL_MODE_DISABLED, getenv("LQ_HIL_MODE"), 0);
    }
    #endif
    
    /* Initialize engine */
    int ret = lq_engine_init(&g_lq_engine);
    if (ret != 0) return ret;
    
    /* Hardware input layer */
    ret = lq_hw_input_init(64);
    if (ret != 0) return ret;
    
    /* Platform-specific peripheral init */
    #ifdef LQ_PLATFORM_INIT
    lq_platform_peripherals_init();
    #endif
    
    return 0;
}

/* Output event dispatcher */
void lq_generated_dispatch_outputs(void) {
    /* Dispatch output events to appropriate protocol drivers/hardware */
    for (size_t i = 0; i < g_lq_engine.out_event_count; i++) {
        struct lq_output_event *evt = &g_lq_engine.out_events[i];
        
        switch (evt->type) {
            case LQ_OUTPUT_J1939: {
                /* J1939 output: encode value and send via CAN */
                uint8_t data[8] = {0};
                data[0] = (uint8_t)(evt->value & 0xFF);
                data[1] = (uint8_t)((evt->value >> 8) & 0xFF);
                data[2] = (uint8_t)((evt->value >> 16) & 0xFF);
                data[3] = (uint8_t)((evt->value >> 24) & 0xFF);
                
                /* Build CAN ID from PGN (target_id) */
                uint32_t can_id = lq_j1939_build_id_from_pgn(evt->target_id, 6, 0);
                lq_can_send(can_id, true, data, 8);
                break;
            }
            default:
                /* Unknown output type - ignore */
                break;
        }
    }
}
