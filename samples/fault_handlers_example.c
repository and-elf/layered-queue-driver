/*
 * User-defined fault response handlers
 * 
 * These override the weak stubs in lq_generated.c
 */

#include "lq_generated.h"
#include <stdio.h>

/* Example: Sensor fault handler */
void sensor_fault_wake(uint8_t monitor_id, int32_t input_value, bool fault_detected)
{
    if (fault_detected) {
        printf("[SAFETY] Sensor fault detected! monitor=%u value=%d\n", 
               monitor_id, input_value);
        
        /* Take immediate safety action:
         * - Switch to backup sensor
         * - Enter limp-home mode
         * - Limit actuator outputs
         * - Trigger warning indicators
         */
        
        // Example: Set safe default values
        // set_actuator_safe_state();
        // enable_limp_mode();
    } else {
        printf("[SAFETY] Sensor fault cleared. monitor=%u\n", monitor_id);
        // Restore normal operation if safe
    }
}

/* Example: Temperature fault handler */
void temperature_fault_wake(uint8_t monitor_id, int32_t input_value, bool fault_detected)
{
    if (fault_detected) {
        printf("[SAFETY] Temperature fault! monitor=%u temp=%d\n", 
               monitor_id, input_value);
        
        /* Critical safety action for overtemperature:
         * - Reduce engine power immediately
         * - Enable cooling fans at max
         * - Trigger emergency shutdown if severe
         */
        
        if (input_value > 1400) {  /* > 140°C - severe */
            printf("[SAFETY] CRITICAL OVERTEMP - EMERGENCY SHUTDOWN\n");
            // emergency_shutdown();
        } else {
            printf("[SAFETY] Overtemp warning - reducing power\n");
            // reduce_power_output(50);  /* 50% power limit */
            // enable_cooling_fans(100); /* Max cooling */
        }
    } else {
        printf("[SAFETY] Temperature fault cleared. monitor=%u\n", monitor_id);
        // Gradually restore power if temperature stable
    }
}
