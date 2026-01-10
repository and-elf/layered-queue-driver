/*
 * Example safe state handler for dual-channel safety system
 * 
 * User must implement enter_safe_state() to define application-specific
 * safe state behavior when event crosscheck fails.
 */

#include "lq_generated.h"
#include "lq_platform.h"
#include <stdio.h>

/**
 * Safe state handler - called when crosscheck fail GPIO goes high
 * 
 * This function is called by the fault monitor when the fail GPIO
 * is asserted (either MCU can trigger it).
 * 
 * Requirements for ASIL-D / SIL3:
 * - Disable all potentially dangerous outputs
 * - Set outputs to known safe values
 * - Enter infinite loop (watchdog will reset if needed)
 * - Do NOT return to normal operation
 */
void enter_safe_state(uint8_t monitor_id, 
                     int32_t input_value,
                     enum lq_fault_level fault_level)
{
    (void)monitor_id;
    (void)input_value;
    (void)fault_level;
    
    /* ========================================
     * Step 1: Disable all outputs immediately
     * ======================================== */
    
    /* Disable PWM outputs (motors, valves) */
    lq_pwm_set(0, 0);  /* Motor 1 */
    lq_pwm_set(1, 0);  /* Motor 2 */
    
    /* Set GPIO outputs to safe state */
    lq_gpio_set(12, false);  /* Brake light off */
    lq_gpio_set(13, false);  /* Main relay off */
    lq_gpio_set(14, true);   /* Alarm LED on */
    
    /* ========================================
     * Step 2: Set safety-critical outputs
     * ======================================== */
    
    /* Example: Apply brakes */
    lq_gpio_set(10, true);   /* Brake solenoid ON */
    
    /* Example: Kill ignition */
    lq_gpio_set(11, false);  /* Ignition OFF */
    
    /* ========================================
     * Step 3: Send diagnostic CAN message
     * ======================================== */
    
    #ifdef LQ_PLATFORM_CAN_ENABLED
    /* Send DM1 fault message */
    uint8_t fault_msg[8] = {
        0xFF,  /* SPN LSB */
        0xFF,  /* SPN MSB */
        0x00,  /* FMI */
        0x00,  /* OC */
        0x01,  /* Critical fault */
        0x00, 0x00, 0x00
    };
    
    /* J1939 DM1 - Active diagnostic trouble codes */
    uint32_t dm1_id = 0x18FECA00;  /* PGN 65226, source 0 */
    lq_can_send(dm1_id, true, fault_msg, 8);
    #endif
    
    /* ========================================
     * Step 4: Log failure information
     * ======================================== */
    
    printf("*** SAFE STATE TRIGGERED ***\n");
    printf("Dual-channel crosscheck failed\n");
    printf("Monitor ID: %u\n", monitor_id);
    printf("Input value: %d\n", input_value);
    printf("Fault level: %u\n", fault_level);
    printf("System entering infinite safe state loop\n");
    
    /* ========================================
     * Step 5: Enter infinite safe state loop
     * ======================================== */
    
    /* CRITICAL: Do NOT return from this function
     * 
     * Options:
     * 1. Infinite loop (recommended for ASIL-D)
     * 2. System reset (if watchdog is enabled)
     * 3. Call platform-specific safe state function
     */
    
    while (1) {
        /* Keep alarm LED blinking to indicate safe state */
        lq_gpio_set(14, true);   /* Alarm LED on */
        lq_platform_sleep_ms(500);
        lq_gpio_set(14, false);  /* Alarm LED off */
        lq_platform_sleep_ms(500);
        
        /* Optional: Feed watchdog to prevent reset
         * (only if you want to stay in safe state indefinitely)
         */
        #ifdef LQ_WATCHDOG_ENABLED
        // feed_watchdog();
        #endif
        
        /* Optional: Check for explicit reset command
         * (e.g., from service technician via diagnostic port)
         */
        #ifdef LQ_ALLOW_SAFE_STATE_RECOVERY
        // if (check_reset_command()) {
        //     system_reset();
        // }
        #endif
    }
    
    /* Should never reach here */
}

/**
 * Optional: External watchdog callback
 * 
 * If using external watchdog that monitors both MCUs,
 * implement this to handle watchdog timeout.
 */
#ifdef LQ_EXTERNAL_WATCHDOG
void watchdog_timeout_handler(void)
{
    /* Both MCUs failed to feed watchdog - hardware reset */
    printf("External watchdog timeout - resetting system\n");
    
    /* Hardware will reset automatically */
    while (1);
}
#endif
