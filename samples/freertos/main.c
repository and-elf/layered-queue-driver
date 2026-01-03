/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * FreeRTOS Sample Application - Clean Architecture
 * 
 * This application demonstrates pure C code with all configuration
 * generated at build-time from devicetree.
 * 
 * Build process:
 *   1. scripts/platform_adaptors.py generates ISRs and init code
 *   2. scripts/dts_gen.py parses app.dts
 *   3. Generates lq_generated.c/h with all tasks, ISRs, and peripherals
 *   4. This file just includes and uses the generated code
 * 
 * No manual ISRs, no manual task creation - all generated.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal.h"
#include "lq_engine.h"
#include "lq_generated.h"
#include "lq_log.h"

LQ_LOG_MODULE_REGISTER(freertos_sample, LQ_LOG_LEVEL_INF);

/* ============================================================================
 * Application Entry Point
 * ========================================================================== */

int main(void)
{
    /* Initialize STM32 HAL (CubeMX generated) */
    HAL_Init();
    SystemClock_Config();
    
    LQ_LOG_INF("FreeRTOS Automotive Engine Monitor");
    LQ_LOG_INF("Engine signals: %u", g_lq_engine.num_signals);
    LQ_LOG_INF("Merge contexts: %u", g_lq_engine.num_merges);
    LQ_LOG_INF("Cyclic outputs: %u", g_lq_engine.num_cyclic_outputs);
    
    /* Initialize hardware and layered queue system (generated) */
    int ret = lq_generated_init();
    if (ret != 0) {
        LQ_LOG_ERR("Failed to initialize: %d", ret);
        Error_Handler();
    }
    
    /* Start the engine task and all cyclic output tasks (generated) */
    ret = lq_engine_start(&g_lq_engine);
    if (ret != 0) {
        LQ_LOG_ERR("Failed to start engine: %d", ret);
        Error_Handler();
    }
    
    LQ_LOG_INF("Engine tasks started - FreeRTOS scheduler running");
    
    /* Start FreeRTOS scheduler - never returns */
    vTaskStartScheduler();
    
    /* Should never reach here */
    Error_Handler();
    return 0;
}

/* ============================================================================
 * FreeRTOS Hooks (optional monitoring)
 * ========================================================================== */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    LQ_LOG_ERR("Stack overflow in task: %s", pcTaskName);
    
    __disable_irq();
    while (1);
}

void vApplicationMallocFailedHook(void)
{
    LQ_LOG_ERR("Heap allocation failed");
    
    __disable_irq();
    while (1);
}

void vApplicationIdleHook(void)
{
    /* Optional: Enter low-power mode in idle task */
    /* __WFI(); */
}

/* ============================================================================
 * Example: Application Logic (optional)
 * 
 * All ISRs and tasks are auto-generated. You can access signals here
 * if you want custom application logic beyond what's in the DTS.
 * ========================================================================== */

void app_monitor_engine(void)
{
    /* Example: Read merged RPM signal (ID from DTS) */
    struct lq_signal *rpm = &g_lq_engine.signals[10];
    
    LQ_LOG_INF("Engine RPM: %d (status=%d, age=%llu us)",
               rpm->value, rpm->status, 
               lq_platform_get_time_us() - rpm->timestamp_us);
}

/* ============================================================================
 * STM32 CubeMX Stubs (implement in your CubeMX generated code)
 * ========================================================================== */

__weak void SystemClock_Config(void) {}
__weak void Error_Handler(void) { while(1); }
