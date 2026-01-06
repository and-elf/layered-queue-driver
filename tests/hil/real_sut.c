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
#include <pthread.h>
#include "lq_hil.h"
#include "lq_platform.h"
#include "lq_engine.h"
#include "lq_hw_input.h"
#include "lq_event.h"
#include "lq_j1939.h"

#ifdef FULL_APP
#include "layered_queue_core.h"
#include "lq_generated.h"
extern int lq_generated_init(void);
extern struct lq_engine g_lq_engine;

/* HIL receiver thread - polls for injections and calls ISRs */
static void *hil_receiver_thread(void *arg) {
    (void)arg;
    
    printf("[SUT-RX] Receiver thread started\n");
    
    while (1) {
        struct lq_hil_adc_msg adc_msg;
        
        /* Poll for ADC injections */
        printf("[SUT-RX] Calling lq_hil_sut_recv_adc...\n");
        int ret = lq_hil_sut_recv_adc(&adc_msg, 1000);
        printf("[SUT-RX] lq_hil_sut_recv_adc returned %d\n", ret);
        if (ret == 0) {
            printf("[SUT-RX] Received ADC ch%d = %u\n", adc_msg.hdr.channel, adc_msg.value);
            /* Call the appropriate ISR based on channel */
            switch (adc_msg.hdr.channel) {
                case 0:
                    lq_adc_isr_rpm_adc(adc_msg.value);
                    printf("[SUT-RX] Called lq_adc_isr_rpm_adc(%u)\n", adc_msg.value);
                    break;
                case 1:
                    lq_spi_isr_rpm_spi(adc_msg.value);
                    printf("[SUT-RX] Called lq_spi_isr_rpm_spi(%u)\n", adc_msg.value);
                    break;
                case 2:
                    lq_adc_isr_temp_adc(adc_msg.value);
                    printf("[SUT-RX] Called lq_adc_isr_temp_adc(%u)\n", adc_msg.value);
                    break;
                default:
                    fprintf(stderr, "[SUT-RX] Unknown ADC channel: %d\n", adc_msg.hdr.channel);
                    break;
            }
        }
    }
    
    return NULL;
}
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
    
    /* Debug cyclic output configuration */
    printf("[SUT] Cyclic outputs configured: %d\n", g_lq_engine.num_cyclic_outputs);
    for (int i = 0; i < g_lq_engine.num_cyclic_outputs; i++) {
        struct lq_cyclic_ctx *ctx = &g_lq_engine.cyclic_outputs[i];
        printf("[SUT]   [%d]: enabled=%d src_sig=%d period=%llu deadline=%llu\n",
               i, ctx->enabled, ctx->source_signal, 
               (unsigned long long)ctx->period_us, 
               (unsigned long long)ctx->next_deadline);
    }
    
    /* Start HIL receiver thread to handle ADC injections */
    pthread_t rx_thread;
    if (pthread_create(&rx_thread, NULL, hil_receiver_thread, NULL) != 0) {
        fprintf(stderr, "[SUT] Failed to create receiver thread\n");
        return 1;
    }
    pthread_detach(rx_thread);
    
    printf("[SUT] Ready for testing\n");
    
    /* Main processing loop - process hardware inputs and outputs */
    while (running) {
        uint64_t now = lq_platform_get_time_us();
        
        /* Pop hardware samples and ingest into engine */
        struct lq_hw_sample samples[16];
        int num_samples = 0;
        struct lq_hw_sample sample;
        while (num_samples < 16 && lq_hw_pop(&sample) == 0) {
            samples[num_samples++] = sample;
        }
        
        if (num_samples > 0) {
            printf("[SUT-MAIN] Popped %d samples from hw queue\n", num_samples);
            /* Convert to events and ingest */
            struct lq_event events[16];
            for (int i = 0; i < num_samples; i++) {
                events[i].source_id = samples[i].src;  /* Source maps directly to signal */
                events[i].value = samples[i].value;
                events[i].timestamp = samples[i].timestamp;
                events[i].status = LQ_EVENT_OK;
                printf("[SUT-MAIN]   Sample[%d]: src=%d value=%u\n", i, samples[i].src, samples[i].value);
            }
            
            lq_ingest_events(&g_lq_engine, events, num_samples);
            printf("[SUT-MAIN] Ingested %d events into engine\n", num_samples);
        }
        
        /* Process merges/voting */
        lq_process_merges(&g_lq_engine, now);
        
        /* Process cyclic outputs (periodic CAN messages) */
        printf("[SUT-MAIN] Calling lq_process_cyclic_outputs (now=%llu, out_count=%zu)\n", 
               (unsigned long long)now, g_lq_engine.out_event_count);
        lq_process_cyclic_outputs(&g_lq_engine, now);
        printf("[SUT-MAIN] After cyclic processing: out_count=%zu\n", g_lq_engine.out_event_count);
        
        /* Send output events */
        for (size_t i = 0; i < g_lq_engine.out_event_count; i++) {
            struct lq_output_event *evt = &g_lq_engine.out_events[i];
            
            if (evt->type == LQ_OUTPUT_J1939) {
                /* Simple encoding: value as 2-byte little-endian */
                uint8_t data[8] = {0};
                data[0] = evt->value & 0xFF;
                data[1] = (evt->value >> 8) & 0xFF;
                
                /* Build CAN ID from PGN */
                uint32_t can_id = lq_j1939_build_id_from_pgn(evt->target_id, 6, 0);
                
                printf("[SUT-OUTPUT] Sending J1939 PGN 0x%X (CAN ID 0x%X) value=%u\n", 
                       evt->target_id, can_id, evt->value);
                lq_can_send(can_id, true, data, 8);
            }
        }
        
        /* Sleep briefly */
        lq_platform_sleep_ms(5);
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
