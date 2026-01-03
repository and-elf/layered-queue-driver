/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Automotive Engine Monitor - Clean Application
 * 
 * This application demonstrates pure C code with all configuration
 * generated at build-time from devicetree.
 * 
 * Build process:
 *   1. scripts/dts_gen.py parses app.dts
 *   2. Generates lq_generated.c/h with engine struct and ISRs
 *   3. This file just includes and uses the generated code
 * 
 * No macros, no RTOS dependencies, just clean C.
 */

#include "lq_engine.h"
#include "lq_generated.h"
#include "lq_log.h"

LQ_LOG_MODULE_REGISTER(automotive, LQ_LOG_LEVEL_INF);

/* ============================================================================
 * Application
 * ========================================================================== */

int main(void)
{
    LQ_LOG_INF("Automotive Engine Monitor - Generated from DTS");
    LQ_LOG_INF("Engine signals: %u", g_lq_engine.num_signals);
    LQ_LOG_INF("Merge contexts: %u", g_lq_engine.num_merges);
    LQ_LOG_INF("Cyclic outputs: %u", g_lq_engine.num_cyclic_outputs);
    
    /* Initialize (hardware input layer + sensor setup) */
    int ret = lq_generated_init();
    if (ret != 0) {
        LQ_LOG_ERR("Failed to initialize: %d", ret);
        return ret;
    }
    
    /* Start the engine task (if configured) */
#ifdef CONFIG_LQ_ENGINE_TASK
    ret = lq_engine_start(&g_lq_engine);
    if (ret != 0) {
        LQ_LOG_ERR("Failed to start engine: %d", ret);
        return ret;
    }
    LQ_LOG_INF("Engine task started");
    
    /* RTOS will handle scheduling */
    return 0;
#else
    /* Manual loop for bare-metal or RTOS-free systems */
    LQ_LOG_INF("Running engine in main loop (no task)");
    
    while (1) {
        lq_engine_step(&g_lq_engine);
        
        /* Platform-specific delay */
        lq_platform_delay_ms(10);
    }
#endif
}

/* ============================================================================
 * Example: Accessing signals (for application logic)
 * ========================================================================== */

void app_read_rpm(void)
{
    /* The merged RPM is in signal 10 (from DTS output-signal-id) */
    struct lq_signal *rpm = &g_lq_engine.signals[10];
    
    LQ_LOG_INF("Engine RPM: %d (status=%d, age=%llu us)",
               rpm->value, rpm->status, 
               lq_platform_get_time_us() - rpm->timestamp_us);
}

void app_read_temp(void)
{
    /* Temperature is in signal 2 (from DTS signal-id) */
    struct lq_signal *temp = &g_lq_engine.signals[2];
    
    LQ_LOG_INF("Engine Temperature: %d°C (status=%d)",
               temp->value, temp->status);
}
