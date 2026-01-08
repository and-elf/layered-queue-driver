/*
 * Simple SUT for HIL testing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "lq_hil.h"

static volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    printf("[SUT] Starting in HIL mode (PID: %d)\n", getpid());
    
    /* Initialize HIL - auto-detects from LQ_HIL_MODE env */
    if (lq_hil_init(LQ_HIL_MODE_DISABLED, getenv("LQ_HIL_MODE"), 0) != 0) {
        fprintf(stderr, "[SUT] HIL init failed\n");
        return 1;
    }
    
    if (!lq_hil_is_active()) {
        printf("[SUT] HIL not active - set LQ_HIL_MODE=sut\n");
        return 1;
    }
    
    printf("[SUT] Ready for testing\n");
    
    /* Simple loop: receive inputs and echo to outputs */
    while (running) {
        struct lq_hil_adc_msg adc_msg;
        struct lq_hil_can_msg can_msg;
        
        /* Check for ADC input */
        if (lq_hil_sut_recv_adc(&adc_msg, 10) == 0) {
            printf("[SUT] RX ADC ch%d = %u\n", adc_msg.hdr.channel, adc_msg.value);
            
            /* Echo on CAN */
            can_msg.hdr.type = LQ_HIL_MSG_CAN;
            can_msg.hdr.timestamp_us = lq_hil_get_timestamp_us();
            can_msg.can_id = 0x18FEF100;  /* EEC1 */
            can_msg.is_extended = 1;
            can_msg.dlc = 8;
            
            /* Put value in first 2 bytes (little-endian) */
            can_msg.data[0] = adc_msg.value & 0xFF;
            can_msg.data[1] = (adc_msg.value >> 8) & 0xFF;
            memset(&can_msg.data[2], 0, 6);
            
            lq_hil_sut_send_can(NULL, &can_msg);
            printf("[SUT] TX CAN EEC1\n");
        }
        
        /* Check for CAN input */
        if (lq_hil_sut_recv_can(NULL, &can_msg, 10) == 0) {
            uint32_t pgn = (can_msg.can_id >> 8) & 0x3FFFF;
            printf("[SUT] RX CAN PGN=%u\n", pgn);
            
            /* Echo it back */
            lq_hil_sut_send_can(NULL, &can_msg);
            printf("[SUT] TX CAN PGN=%u\n", pgn);
        }
        
        usleep(10000);  /* 10ms */
    }
    
    printf("[SUT] Shutting down\n");
    lq_hil_cleanup();
    
    return 0;
}
