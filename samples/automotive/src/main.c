/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Automotive Engine Monitor - Fully Auto-Generated
 * 
 * This sample demonstrates complete code generation from devicetree:
 * - Engine struct initialization
 * - ISR handlers for ADC/SPI inputs
 * - Merge configurations
 * - Cyclic output scheduling
 * 
 * The devicetree (app.dts) defines the complete system.
 * This C file just includes the generated code and starts the engine.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include "lq_engine.h"
#include "lq_hw_input.h"
#include "lq_devicetree.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(automotive_sample, LOG_LEVEL_INF);

/* ============================================================================
 * AUTO-GENERATED: ISR and Trigger Handlers from devicetree
 * ========================================================================== */

/* Generate ISR handlers for all ADC inputs */
LQ_FOREACH_HW_ADC_INPUT(LQ_GEN_ADC_ISR_HANDLER)

/* Generate ISR handlers for all SPI inputs */
LQ_FOREACH_HW_SPI_INPUT(LQ_GEN_SPI_ISR_HANDLER)

/* Generate trigger handlers for all sensor inputs (TMP117, BME280, LSM6DSL, etc.) */
LQ_FOREACH_HW_SENSOR_INPUT(LQ_GEN_SENSOR_TRIGGER_HANDLER)

/* ============================================================================
 * AUTO-GENERATED: Engine Initialization from devicetree
 * ========================================================================== */

/* This single line generates the complete engine struct with:
 * - All signals initialized
 * - All merge contexts configured
 * - All cyclic outputs configured
 */
static struct lq_engine engine = LQ_ENGINE_DT_INIT(DT_NODELABEL(engine));

/* ============================================================================
 * ISR Setup (still needs some boilerplate, but much reduced)
 * ========================================================================== */

static int setup_hardware_inputs(void)
{
    /* Setup ADC channels */
    LQ_FOREACH_HW_ADC_INPUT(LQ_GEN_ADC_ISR_SETUP)
    
    /* Setup sensor triggers (TMP117, BME280, LSM6DSL, ICP10125, etc.) */
    LQ_FOREACH_HW_SENSOR_INPUT(LQ_GEN_SENSOR_TRIGGER_SETUP)
    
    LOG_INF("Hardware inputs and sensors configured");
    return 0;
}

/* ============================================================================
 * Main Application
 * ========================================================================== */

int main(void)
{
    LOG_INF("Automotive Engine Monitor - Auto-Generated Sample");
    LOG_INF("Engine signals: %u", engine.num_signals);
    LOG_INF("Merge contexts: %u", engine.num_merges);
    LOG_INF("Cyclic outputs: %u", engine.num_cyclic_outputs);
    
    /* Initialize hardware input ringbuffer */
    int ret = lq_hw_input_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize hardware input: %d", ret);
        return ret;
    }
    
    /* Setup ADC/SPI ISR handlers */
    ret = setup_hardware_inputs();
    if (ret != 0) {
        LOG_ERR("Failed to setup hardware: %d", ret);
        return ret;
    }
    
    /* Start the engine task (if configured) */
#ifdef CONFIG_LQ_ENGINE_TASK
    ret = lq_engine_start(&engine);
    if (ret != 0) {
        LOG_ERR("Failed to start engine: %d", ret);
        return ret;
    }
    LOG_INF("Engine task started");
#else
    /* Manual loop if task is disabled */
    LOG_INF("Running engine in main loop (no task)");
    while (1) {
        lq_engine_step(&engine);
        k_sleep(K_MSEC(10));
    }
#endif
    
    return 0;
}

/* ============================================================================
 * Example: Accessing generated data
 * ========================================================================== */

void example_read_rpm(void)
{
    /* The merged RPM is in signal 10 (from DTS output-signal-id) */
    struct lq_signal *rpm = &engine.signals[10];
    
    LOG_INF("Engine RPM: %d (status=%d, age=%llu us)",
            rpm->value, rpm->status, 
            lq_platform_get_time_us() - rpm->timestamp_us);
}

void example_read_temp(void)
{
    /* Temperature is in signal 2 (from DTS signal-id) */
    struct lq_signal *temp = &engine.signals[2];
    
    LOG_INF("Engine Temperature: %d°C (status=%d)",
            temp->value, temp->status);
}

/* 
 * Key Benefits of this approach:
 * 
 * 1. DTS references ACTUAL sensor drivers (TMP117, BME280, LSM6DSL, ICP10125)
 * 2. Sensor triggers auto-configured from DTS
 * 3. Device initialization order enforced via init-priority
 * 4. ISR handlers auto-generated for all inputs
 * 5. Engine struct auto-initialized from DTS
 * 6. Signal IDs are compile-time constants
 * 7. Easy to swap sensors - just change sensor-device phandle
 * 
 * Example sensor driver integration:
 * - TMP117: High-precision I2C temp sensor (±0.1°C)
 * - BME280: Environmental sensor (temp, humidity, pressure)
 * - LSM6DSL: 6-axis IMU for vibration monitoring
 * - ICP10125: High-accuracy barometric pressure
 * 
 * Init priority ordering:
 * - 50-59: I2C/SPI/CAN bus drivers
 * - 60-69: Sensor drivers (TMP117, BME280, etc.)
 * - 70-79: Application drivers
 * - 80:    Hardware input layer (our ISR handlers)
 * - 85:    Engine processing layer
 * - 90+:   Application tasks
 */
