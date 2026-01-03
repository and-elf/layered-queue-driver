/*
 * Real SUT for HIL testing - demonstrates how the actual application runs
 * 
 * NOTE: This requires up-to-date generated code. For now, we use this as
 * a template showing how HIL intercepts platform calls.
 * 
 * To use with actual generated code:
 * 1. Generate code with scripts/dts_gen.py
 * 2. Build with lq_platform_hil.c instead of lq_platform_native.c
 * 3. All ADC/CAN/GPIO calls automatically route through HIL
 */

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "lq_hil.h"
#include "lq_platform.h"

/*
 * When you integrate with real generated code, uncomment these:
 * 
 * extern int lq_generated_init(void);
 * extern struct lq_engine g_lq_engine;
 */

static volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    printf("[SUT] HIL Platform Demo (PID: %d)\n", getpid());
    printf("[SUT] This demonstrates how platform calls are intercepted\n");
    
    /* Initialize HIL - auto-detects from LQ_HIL_MODE env */
    if (lq_hil_init(LQ_HIL_MODE_DISABLED, 0) != 0) {
        fprintf(stderr, "[SUT] HIL init failed\n");
        return 1;
    }
    
    if (!lq_hil_is_active()) {
        printf("[SUT] HIL not active - set LQ_HIL_MODE=sut\n");
        return 1;
    }
    
    printf("[SUT] HIL active - platform calls intercepted\n");
    
    /*
     * In real application, you would call:
     * 
     * int ret = lq_generated_init();
     * if (ret != 0) {
     *     fprintf(stderr, "[SUT] lq_generated_init() failed: %d\n", ret);
     *     return 1;
     * }
     * 
     * ret = lq_engine_start(&g_lq_engine);
     * if (ret != 0) {
     *     fprintf(stderr, "[SUT] lq_engine_start() failed: %d\n", ret);
     *     return 1;
     * }
     */
    
    printf("[SUT] Demonstrating platform interception:\n");
    printf("[SUT] - lq_adc_read() → receives from HIL tester\n");
    printf("[SUT] - lq_can_send() → sends to HIL tester\n");
    printf("[SUT] - lq_gpio_set() → notifies HIL tester\n");
    printf("[SUT] Ready for testing\n");
    
    /* Simple test loop */
    while (running) {
        uint16_t adc_value;
        
        /* Try to read ADC - will get data from HIL tester */
        if (lq_adc_read(0, &adc_value) == 0) {
            printf("[SUT] ADC read: ch0 = %u\n", adc_value);
            
            /* Send on CAN - goes to HIL tester for verification */
            uint8_t can_data[8] = {
                (adc_value >> 8) & 0xFF,
                adc_value & 0xFF,
                0, 0, 0, 0, 0, 0
            };
            lq_can_send(0x18FEF100, true, can_data, 8);
            printf("[SUT] CAN sent: ID=0x18FEF100 data[0:1]=%02X%02X\n",
                   can_data[0], can_data[1]);
        }
        
        lq_platform_sleep_ms(10);
    }
    
    printf("[SUT] Shutting down...\n");
    lq_hil_cleanup();
    
    return 0;
}
