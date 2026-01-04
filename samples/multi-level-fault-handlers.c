/*
 * Multi-Level Fault Handler Example
 * 
 * Demonstrates differentiated safety responses based on fault severity:
 *   Level 0 (Info):     Diagnostic logging, no action required
 *   Level 1 (Warning):  Soft fault - reduce performance, continue operation
 *   Level 2 (Error):    Moderate fault - enter degraded/limp mode
 *   Level 3 (Critical): Hard fault - emergency shutdown
 */

#include "lq_generated.h"
#include "lq_common.h"
#include <stdio.h>
#include <stdbool.h>

/* System state tracking */
static bool degraded_mode = false;
static bool emergency_shutdown_active = false;
static uint8_t power_limit_percent = 100;

/*
 * Info Level: Staleness detection
 * 
 * When temperature data becomes stale, log for diagnostics but don't
 * take safety action - other monitors will catch real faults.
 */
void temp_stale_wake(uint8_t monitor_id, int32_t temp, enum lq_fault_level level)
{
    if (level == LQ_FAULT_LEVEL_0) {
        /* This is actually a fault at info level - stale data detected */
        printf("[INFO] Temperature data stale (monitor %u)\n", monitor_id);
        printf("[INFO] Last value was %d, age exceeded 500ms\n", temp);
        /* No safety action - just diagnostic logging */
    }
    /* Note: Level 0 can also mean "fault cleared" for higher-level monitors */
}

/*
 * Warning Level: 110°C threshold
 * 
 * Soft fault - take preventive action but continue operation.
 * Goal is to prevent escalation to error/critical levels.
 */
void temp_warning_wake(uint8_t monitor_id, int32_t temp, enum lq_fault_level level)
{
    if (level >= LQ_FAULT_LEVEL_1) {
        printf("[WARNING] Temperature elevated: %d.%d°C (monitor %u, level %d)\n",
               temp / 10, temp % 10, monitor_id, level);
        
        /* Soft fault response: */
        /* - Reduce power output to 80% */
        /* - Enable cooling fans at high speed */
        /* - Continue normal operation with reduced capability */
        
        if (power_limit_percent > 80) {
            power_limit_percent = 80;
            printf("[ACTION] Reducing power limit to 80%%\n");
            // set_power_limit(80);
        }
        
        printf("[ACTION] Enabling high-speed cooling\n");
        // set_cooling_fans(80);  /* 80% fan speed */
        
    } else if (level == LQ_FAULT_LEVEL_0) {
        printf("[WARNING] Temperature normal, warning cleared (monitor %u)\n", monitor_id);
        
        /* Restore power if no higher-level faults active */
        if (!degraded_mode && power_limit_percent < 100) {
            power_limit_percent = 100;
            printf("[ACTION] Restoring power limit to 100%%\n");
            // set_power_limit(100);
        }
    }
}

/*
 * Error Level: 120°C threshold
 * 
 * Moderate fault - enter degraded/limp mode.
 * Significant capability reduction but maintain basic function.
 */
void temp_error_wake(uint8_t monitor_id, int32_t temp, enum lq_fault_level level)
{
    if (level >= LQ_FAULT_LEVEL_2) {
        printf("[ERROR] Temperature high: %d.%d°C (monitor %u, level %d)\n",
               temp / 10, temp % 10, monitor_id, level);
        
        /* Moderate fault response: */
        /* - Enter limp/degraded mode */
        /* - Reduce power to 50% */
        /* - Maximum cooling */
        /* - Disable non-essential functions */
        
        if (!degraded_mode) {
            degraded_mode = true;
            printf("[ACTION] ENTERING DEGRADED MODE\n");
            // set_degraded_mode(true);
        }
        
        power_limit_percent = 50;
        printf("[ACTION] Reducing power limit to 50%%\n");
        // set_power_limit(50);
        
        printf("[ACTION] Enabling maximum cooling\n");
        // set_cooling_fans(100);  /* Max cooling */
        
        printf("[ACTION] Disabling non-essential systems\n");
        // disable_non_essential_systems();
        
    } else if (level == LQ_FAULT_LEVEL_0) {
        printf("[ERROR] Temperature acceptable, error cleared (monitor %u)\n", monitor_id);
        
        /* Can exit degraded mode if critical fault also cleared */
        if (degraded_mode && !emergency_shutdown_active) {
            degraded_mode = false;
            printf("[ACTION] Exiting degraded mode\n");
            // set_degraded_mode(false);
        }
    }
}

/*
 * Critical Level: 130°C threshold
 * 
 * Hard fault - immediate emergency shutdown required.
 * This is a safety-critical response to prevent damage or danger.
 */
void temp_critical_wake(uint8_t monitor_id, int32_t temp, enum lq_fault_level level)
{
    if (level >= LQ_FAULT_LEVEL_3) {
        printf("[CRITICAL] *** EMERGENCY: Temperature critical: %d.%d°C ***\n",
               temp / 10, temp % 10);
        printf("[CRITICAL] monitor %u, level %d\n", monitor_id, level);
        
        /* Hard fault - immediate emergency shutdown: */
        /* - Disable all power outputs immediately */
        /* - Trigger emergency stop */
        /* - Activate emergency cooling */
        /* - Set safe state */
        /* - Notify safety system */
        
        emergency_shutdown_active = true;
        degraded_mode = true;
        power_limit_percent = 0;
        
        printf("[CRITICAL] *** INITIATING EMERGENCY SHUTDOWN ***\n");
        // emergency_shutdown();
        // disable_all_power_outputs();
        // set_emergency_state();
        // activate_emergency_cooling();
        // notify_safety_system(SAFETY_CRITICAL_OVERTEMP);
        
    } else if (level == LQ_FAULT_LEVEL_0) {
        printf("[CRITICAL] Temperature safe, critical fault cleared (monitor %u)\n", monitor_id);
        
        /* Critical fault cleared - but remain in safe state */
        /* Require manual intervention to restart */
        emergency_shutdown_active = false;
        
        printf("[CRITICAL] Emergency cleared but manual reset required\n");
        printf("[CRITICAL] System remains in safe state\n");
        // require_manual_reset();
    }
}

/*
 * Error Level: Sensor voting failure
 * 
 * Redundant sensors disagree - cannot trust temperature reading.
 * This is a moderate fault requiring degraded operation.
 */
void merge_fault_wake(uint8_t monitor_id, int32_t temp, enum lq_fault_level level)
{
    if (level >= LQ_FAULT_LEVEL_2) {
        printf("[ERROR] Sensor voting failure (monitor %u, level %d)\n",
               monitor_id, level);
        printf("[ERROR] Temperature sensors disagree, value=%d\n", temp);
        
        /* Sensor disagreement response: */
        /* - Cannot trust temperature reading */
        /* - Enter degraded mode as precaution */
        /* - Reduce power conservatively */
        /* - May need to select backup sensor or default to safe value */
        
        if (!degraded_mode) {
            degraded_mode = true;
            printf("[ACTION] Entering degraded mode - sensor trust lost\n");
            // set_degraded_mode(true);
        }
        
        if (power_limit_percent > 60) {
            power_limit_percent = 60;
            printf("[ACTION] Reducing power to 60%% (conservative limit)\n");
            // set_power_limit(60);
        }
        
        printf("[ACTION] Attempting sensor diagnostics\n");
        // run_sensor_diagnostics();
        
    } else if (level == LQ_FAULT_LEVEL_0) {
        printf("[ERROR] Sensor voting restored (monitor %u)\n", monitor_id);
        
        /* Sensors agree again - can restore trust */
        if (degraded_mode && !emergency_shutdown_active) {
            degraded_mode = false;
            printf("[ACTION] Exiting degraded mode - sensor trust restored\n");
            // set_degraded_mode(false);
        }
    }
}
