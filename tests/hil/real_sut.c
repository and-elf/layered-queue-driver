/*
 * Real SUT for HIL testing - demonstrates how the actual application runs
 * 
 * This can run in two modes:
 * 1. Demo mode (default): Shows platform interception concept
 * 2. Full mode (with FULL_APP): Runs actual generated application
 */

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "lq_hil.h"
#include "lq_platform.h"

#ifdef FULL_APP
#include "layered_queue_core.h"
extern int lq_generated_init(void);
extern struct lq_engine g_lq_engine;
#endif

static volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
#ifdef FULL_APP
    printf("[SUT] Starting REAL application in HIL mode (PID: %d)\n", getpid());
#else
    printf("[SUT] HIL Platform Demo (PID: %d)\n", getpid());
    printf("[SUT] This demonstrates how platform calls are intercepted\n");
#endif
    
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
    
#ifdef FULL_APP
    /* Run the REAL generated application */
    printf("[SUT] Initializing generated system...\n");
    
    int ret = lq_generated_init();
    if (ret != 0) {
        fprintf(stderr, "[SUT] lq_generated_init() failed: %d\n", ret);
        return 1;
    }
    
    printf("[SUT] Generated system initialized\n");
    printf("[SUT] Engine ready - all hardware I/O goes through HIL\n");
    printf("[SUT] Ready for testing\n");
    
    /* Keep running until signal */
    while (running) {
        lq_platform_sleep_ms(100);
    }
#else
    /* Demo mode - show platform interception */
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
#endif
    
    printf("[SUT] Shutting down...\n");
    lq_hil_cleanup();
    
    return 0;
}
