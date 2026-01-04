/*
 * User-defined fault response handlers
 * 
 * These override the weak stubs in lq_generated.c
 */

#include "lq_generated.h"
#include "lq_common.h"
#include <stdio.h>

/* Example: Sensor fault handler 
 * 
 * Configured with fault-level = <2> (Error)
 * Responds to merged sensor voting failures
 */
void sensor_fault_wake(uint8_t monitor_id, int32_t input_value, enum lq_fault_level level)
{
    if (level >= LQ_FAULT_LEVEL_2) {
        printf("[SAFETY] Sensor fault detected! monitor=%u value=%d level=%d\n", 
               monitor_id, input_value, level);
        
        /* Take immediate safety action:
         * - Switch to backup sensor
         * - Enter limp-home mode
         * - Limit actuator outputs
         * - Trigger warning indicators
         */
        
        // Example: Set safe default values
        // set_actuator_safe_state();
        // enable_limp_mode();
    } else if (level == LQ_FAULT_LEVEL_0) {
        printf("[SAFETY] Sensor fault cleared. monitor=%u\n", monitor_id);
        // Restore normal operation if safe
    }
}

/* Example: Temperature fault handler 
 * 
 * Configured with fault-level = <3> (Critical)
 * Responds to overtemperature conditions with emergency actions
 */
void temperature_fault_wake(uint8_t monitor_id, int32_t input_value, enum lq_fault_level level)
{
    if (level >= LQ_FAULT_LEVEL_3) {
        printf("[SAFETY] CRITICAL Temperature fault! monitor=%u temp=%d level=%d\n", 
               monitor_id, input_value, level);
        
        /* Critical safety action for overtemperature:
         * - Emergency shutdown
         * - Disable all heat sources immediately
         */
        
        printf("[SAFETY] CRITICAL OVERTEMP - EMERGENCY SHUTDOWN\n");
        // emergency_shutdown();
        // disable_all_heaters();
        
    } else if (level >= LQ_FAULT_LEVEL_2) {
        printf("[SAFETY] Temperature error! monitor=%u temp=%d level=%d\n", 
               monitor_id, input_value, level);
        
        /* Error level - reduce power and increase cooling */
        // reduce_power_output(50);  /* 50% power limit */
        // enable_cooling_fans(100); /* Max cooling */
        
    } else if (level == LQ_FAULT_LEVEL_0) {
        printf("[SAFETY] Temperature fault cleared. monitor=%u\n", monitor_id);
        // Gradually restore power if temperature stable
    }
}
