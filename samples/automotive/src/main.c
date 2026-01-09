/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * Platform layer (lq_platform_*.c) handles:
 * - FreeRTOS: Creates task and starts scheduler
 * - Zephyr: Creates thread
 * - Native/Bare metal: Runs infinite loop
 */

#include "lq_engine.h"
#include "lq_generated.h"
#include "lq_platform.h"
#include <stdio.h>

int main(void)
{
    printf("Layered Queue Application\n");
    printf("Signals: %u, Merges: %u, Cyclic: %u\n",
           g_lq_engine.num_signals,
           g_lq_engine.num_merges,
           g_lq_engine.num_cyclic_outputs);
    
    /* Initialize engine and platform */
    int ret = lq_generated_init();
    if (ret != 0) {
        printf("ERROR: Initialization failed: %d\n", ret);
        return ret;
    }
    
    printf("Initialization complete\n");
    
    /* Start engine - platform layer handles tasks/threads/loop */
    return lq_engine_run();
}
