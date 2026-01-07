/*
 * Configuration Framework Example
 * 
 * This example demonstrates how to use the configuration framework
 * to enable runtime configuration of remap and scale drivers via
 * UDS diagnostic protocol over CAN.
 * 
 * Features demonstrated:
 * - Configuration registry setup
 * - UDS DID integration
 * - Runtime parameter modification via diagnostic protocol
 * - Calibration mode for secure configuration
 */

#include "lq_config.h"
#include "lq_uds_can.h"
#include "lq_engine.h"
#include <stdio.h>

/* ============================================================================
 * Global Storage
 * ============================================================================ */

static struct lq_engine engine;
static struct lq_config_registry config_registry;
static struct lq_uds_can_ctx uds_can;

/* Remap and scale configuration arrays */
static struct lq_remap_ctx remaps[4];
static struct lq_scale_ctx scales[4];

/* UDS CAN transport */
static struct lq_isotp_channel isotp_channel;

/* ============================================================================
 * UDS DID Callback Bridge
 * ============================================================================ */

static int uds_read_did_callback(uint16_t did, uint8_t *data, size_t max_len, size_t *actual_len)
{
    return lq_config_uds_read_did(&config_registry, did, data, max_len, actual_len);
}

static int uds_write_did_callback(uint16_t did, const uint8_t *data, size_t len)
{
    return lq_config_uds_write_did(&config_registry, did, data, len);
}

static int uds_routine_control_callback(uint16_t rid, uint8_t control_type,
                                       const uint8_t *in_data, size_t in_len,
                                       uint8_t *out_data, size_t max_out,
                                       size_t *actual_out)
{
    return lq_config_uds_routine_control(&config_registry, rid, control_type,
                                        in_data, in_len, out_data, max_out, actual_out);
}

/* ============================================================================
 * Application Setup
 * ============================================================================ */

int main(void)
{
    printf("Configuration Framework Example\n");
    printf("================================\n\n");
    
    /* Initialize engine */
    memset(&engine, 0, sizeof(engine));
    
    /* Initialize configuration registry */
    lq_config_init(&config_registry, &engine, remaps, 4, scales, 4);
    
    /* Add initial remap: joystick X (signal 0) -> steering (signal 10) */
    struct lq_remap_ctx initial_remap = {
        .input_signal = 0,
        .output_signal = 10,
        .invert = false,
        .deadzone = 50,  /* Ignore small movements */
        .enabled = true,
    };
    
    uint8_t remap_idx;
    if (lq_config_add_remap(&config_registry, &initial_remap, &remap_idx) == 0) {
        printf("Added initial remap: signal %d -> %d (deadzone=%d)\n",
               initial_remap.input_signal, initial_remap.output_signal,
               initial_remap.deadzone);
    }
    
    /* Add initial scale: throttle pedal (signal 1) -> engine demand (signal 11) */
    struct lq_scale_ctx initial_scale = {
        .input_signal = 1,
        .output_signal = 11,
        .scale_factor = 100,   /* 1:1 scaling (100/100) */
        .offset = 0,
        .clamp_min = 0,
        .clamp_max = 10000,    /* Max throttle limit */
        .has_clamp_min = true,
        .has_clamp_max = true,
        .enabled = true,
    };
    
    uint8_t scale_idx;
    if (lq_config_add_scale(&config_registry, &initial_scale, &scale_idx) == 0) {
        printf("Added initial scale: signal %d -> %d (factor=%d, clamp_max=%d)\n",
               initial_scale.input_signal, initial_scale.output_signal,
               initial_scale.scale_factor, initial_scale.clamp_max);
    }
    
    printf("\nConfiguration registry initialized:\n");
    printf("  Remaps: %d/%d\n", config_registry.num_remaps, config_registry.max_remaps);
    printf("  Scales: %d/%d\n", config_registry.num_scales, config_registry.max_scales);
    printf("  Version: %u\n", config_registry.config_version);
    printf("  Calibration mode: %s\n", config_registry.calibration_mode ? "YES" : "NO");
    printf("  Locked: %s\n\n", config_registry.config_locked ? "YES" : "NO");
    
    /* ========================================================================
     * UDS Diagnostic Session Example
     * ======================================================================== */
    
    printf("UDS Diagnostic Protocol Integration\n");
    printf("------------------------------------\n\n");
    
    /* Initialize UDS-CAN with configuration callbacks */
    struct lq_uds_config uds_config = {
        .max_security_level = UDS_SECURITY_LEVEL_2,
        .default_p2_ms = 50,
        .default_p2_star_ms = 5000,
        .s3_timeout_ms = 5000,
        .seed_key_fn = NULL,  /* Use default seed/key */
        .read_did = uds_read_did_callback,
        .write_did = uds_write_did_callback,
        .routine_control = uds_routine_control_callback,
    };
    
    /* Note: In real application, initialize ISO-TP channel with CAN driver */
    printf("UDS server configured with config framework callbacks\n");
    printf("  Read DID:  0xF1A0-0xF1A6 (signals, remap, scale, status)\n");
    printf("  Write DID: 0xF1A2-0xF1A3 (remap, scale)\n");
    printf("  Routines:  0xF1A0-0xF1A2 (calibration, reset)\n\n");
    
    /* ========================================================================
     * Example 1: Read Signal Value via UDS
     * ======================================================================== */
    
    printf("Example 1: Read signal value via UDS DID 0xF1A0\n");
    
    /* Set a test signal */
    engine.signals[5].value = 12345;
    engine.signals[5].status = LQ_EVENT_OK;
    
    /* Simulate UDS read request */
    uint8_t read_data[10];
    size_t read_len;
    
    read_data[0] = 5;  /* Signal ID parameter */
    
    int ret = lq_config_uds_read_did(&config_registry, LQ_DID_SIGNAL_VALUE,
                                    read_data, sizeof(read_data), &read_len);
    
    if (ret == 0) {
        int32_t value = ((int32_t)read_data[0] << 24) |
                       ((int32_t)read_data[1] << 16) |
                       ((int32_t)read_data[2] << 8) |
                       read_data[3];
        printf("  Signal 5 value: %d (0x%08X)\n", value, value);
        printf("  Response length: %zu bytes\n\n", read_len);
    }
    
    /* ========================================================================
     * Example 2: Read Remap Configuration
     * ======================================================================== */
    
    printf("Example 2: Read remap config via UDS DID 0xF1A2\n");
    
    read_data[0] = 0;  /* Remap index */
    ret = lq_config_uds_read_did(&config_registry, LQ_DID_REMAP_CONFIG,
                                read_data, sizeof(read_data), &read_len);
    
    if (ret == 0) {
        printf("  Remap[0] serialized:\n");
        printf("    Input signal:  %d\n", read_data[0]);
        printf("    Output signal: %d\n", read_data[1]);
        printf("    Flags: 0x%02X (invert=%d, enabled=%d)\n",
               read_data[2],
               (read_data[2] & 0x01) ? 1 : 0,
               (read_data[2] & 0x02) ? 1 : 0);
        printf("    Deadzone: %d\n", ((int16_t)read_data[3] << 8) | read_data[4]);
        printf("    Total: %zu bytes\n\n", read_len);
    }
    
    /* ========================================================================
     * Example 3: Modify Scale Configuration (Requires Calibration Mode)
     * ======================================================================== */
    
    printf("Example 3: Modify scale config via UDS\n");
    
    /* First, enter calibration mode via routine */
    printf("  Step 1: Enter calibration mode (RID 0xF1A0)\n");
    
    uint8_t routine_out[10];
    size_t routine_out_len;
    
    ret = lq_config_uds_routine_control(&config_registry, LQ_RID_ENTER_CALIBRATION,
                                       LQ_UDS_ROUTINE_START, NULL, 0,
                                       routine_out, sizeof(routine_out), &routine_out_len);
    
    if (ret == 0) {
        printf("  Calibration mode: ACTIVE\n\n");
        
        /* Now modify scale config */
        printf("  Step 2: Write scale config (DID 0xF1A3)\n");
        
        uint8_t write_data[20];
        write_data[0] = 0;      /* Scale index */
        write_data[1] = 1;      /* input_signal */
        write_data[2] = 11;     /* output_signal */
        write_data[3] = 0x07;   /* flags: enabled | has_clamp_min | has_clamp_max */
        write_data[4] = 0x00;   /* scale_factor high (150 = 0x0096) */
        write_data[5] = 0x96;   /* scale_factor low */
        write_data[6] = 0x00;   /* offset high */
        write_data[7] = 0x00;   /* offset low */
        write_data[8] = 0x00;   /* clamp_min (0) */
        write_data[9] = 0x00;
        write_data[10] = 0x00;
        write_data[11] = 0x00;
        write_data[12] = 0x00;  /* clamp_max (8000 = 0x00001F40) */
        write_data[13] = 0x00;
        write_data[14] = 0x1F;
        write_data[15] = 0x40;
        
        ret = lq_config_uds_write_did(&config_registry, LQ_DID_SCALE_CONFIG,
                                     write_data, 16);
        
        if (ret == 0) {
            printf("  Scale[0] updated successfully\n");
            printf("    New scale_factor: 150 (1.5x scaling)\n");
            printf("    New clamp_max: 8000 (reduced from 10000)\n");
            printf("    Config version: %u\n\n", config_registry.config_version);
            
            /* Verify the change */
            struct lq_scale_ctx updated_scale;
            lq_config_read_scale(&config_registry, 0, &updated_scale);
            printf("  Verified: scale_factor=%d, clamp_max=%d\n\n",
                   updated_scale.scale_factor, updated_scale.clamp_max);
        }
        
        /* Exit calibration mode */
        printf("  Step 3: Exit calibration mode (RID 0xF1A1)\n");
        ret = lq_config_uds_routine_control(&config_registry, LQ_RID_EXIT_CALIBRATION,
                                           LQ_UDS_ROUTINE_START, NULL, 0,
                                           routine_out, sizeof(routine_out), &routine_out_len);
        
        if (ret == 0) {
            printf("  Configuration locked\n");
            printf("  Calibration mode: INACTIVE\n\n");
        }
    }
    
    /* ========================================================================
     * Example 4: Read System Status
     * ======================================================================== */
    
    printf("Example 4: Read calibration mode status (DID 0xF1A6)\n");
    
    ret = lq_config_uds_read_did(&config_registry, LQ_DID_CALIBRATION_MODE,
                                read_data, sizeof(read_data), &read_len);
    
    if (ret == 0) {
        printf("  Calibration mode: %s\n", read_data[0] ? "ACTIVE" : "INACTIVE");
        printf("  Config locked: %s\n", read_data[1] ? "YES" : "NO");
        printf("  Num remaps: %d\n", read_data[2]);
        printf("  Num scales: %d\n", read_data[3]);
        
        uint32_t version = ((uint32_t)read_data[4] << 24) |
                          ((uint32_t)read_data[5] << 16) |
                          ((uint32_t)read_data[6] << 8) |
                          read_data[7];
        printf("  Config version: %u\n\n", version);
    }
    
    /* ========================================================================
     * Summary
     * ======================================================================== */
    
    printf("Summary\n");
    printf("-------\n");
    printf("The configuration framework provides:\n");
    printf("  ✓ Runtime access to remap/scale parameters via UDS DIDs\n");
    printf("  ✓ Secure calibration mode for parameter modification\n");
    printf("  ✓ Signal value/status monitoring (DID 0xF1A0, 0xF1A1)\n");
    printf("  ✓ Configuration serialization for CAN transport\n");
    printf("  ✓ Version tracking for change detection\n");
    printf("  ✓ Validation of signal IDs and parameter ranges\n\n");
    
    printf("Typical diagnostic workflow:\n");
    printf("  1. UDS Session Control (0x10 0x03) - Enter extended session\n");
    printf("  2. Security Access (0x27 0x03) - Unlock security level 2\n");
    printf("  3. Routine Control (0x31 0x01 0xF1A0) - Enter calibration mode\n");
    printf("  4. Write Data By ID (0x2E 0xF1A2/0xF1A3) - Modify configs\n");
    printf("  5. Routine Control (0x31 0x01 0xF1A1) - Exit and lock\n\n");
    
    return 0;
}
