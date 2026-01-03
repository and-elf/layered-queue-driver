/*
 * FreeRTOS Sample Application for Layered Queue Driver
 * 
 * Demonstrates:
 * - FreeRTOS task integration
 * - ISR-safe data pushing
 * - Event-driven processing
 * - Cyclic output tasks
 * - J1939 CAN transmission
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"
#include "lq_generated.h"
#include "lq_platform.h"
#include "lq_j1939.h"

/* External HAL handles (configured by CubeMX) */
extern ADC_HandleTypeDef hadc1;
extern CAN_HandleTypeDef hcan1;

/* Task handles */
static TaskHandle_t processing_task_handle;
static TaskHandle_t diagnostics_task_handle;

/* Error counters for DM1 */
static uint8_t error_count_coolant_temp = 0;
static uint8_t error_count_oil_pressure = 0;

/* Processing task - handles sensor fusion and voting */
void lq_processing_task(void *pvParameters)
{
    (void)pvParameters;
    
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(50);  /* 20Hz base rate */
    
    while (1) {
        /* Wait for new data with timeout */
        if (lq_platform_wait_event(50000) == 0) {  /* 50ms timeout */
            /* Process hardware inputs */
            lq_hw_input_process();
            
            /* Process merge/voting layer */
            lq_merge_process();
            
            /* Check for errors */
            struct lq_signal *coolant_sig = lq_get_signal(3);  /* Coolant temp */
            struct lq_signal *oil_sig = lq_get_signal(2);      /* Oil pressure */
            
            if (coolant_sig && coolant_sig->value > 110000) {  /* > 110°C */
                error_count_coolant_temp++;
            }
            
            if (oil_sig && oil_sig->value < 20000) {  /* < 20 kPa */
                error_count_oil_pressure++;
            }
        }
        
        /* Run periodically even without events */
        vTaskDelayUntil(&last_wake, period);
    }
}

/* Diagnostics task - sends J1939 DM1 messages */
void lq_diagnostics_task(void *pvParameters)
{
    (void)pvParameters;
    
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000);  /* 1Hz */
    
    lq_j1939_dm1_t dm1 = {0};
    
    while (1) {
        /* Build DM1 message if errors exist */
        dm1.dtc_count = 0;
        dm1.malfunction_lamp = J1939_LAMP_OFF;
        dm1.amber_warning_lamp = J1939_LAMP_OFF;
        dm1.red_stop_lamp = J1939_LAMP_OFF;
        
        if (error_count_coolant_temp > 0) {
            /* SPN 110 = Coolant temp, FMI 0 = Above normal */
            dm1.dtc_list[dm1.dtc_count++] = lq_j1939_create_dtc(110, 0, error_count_coolant_temp);
            dm1.amber_warning_lamp = J1939_LAMP_ON;
            dm1.malfunction_lamp = J1939_LAMP_ON;
        }
        
        if (error_count_oil_pressure > 0) {
            /* SPN 100 = Oil pressure, FMI 1 = Below normal */
            dm1.dtc_list[dm1.dtc_count++] = lq_j1939_create_dtc(100, 1, error_count_oil_pressure);
            dm1.red_stop_lamp = J1939_LAMP_ON;  /* Critical! */
            dm1.malfunction_lamp = J1939_LAMP_ON;
        }
        
        /* Transmit DM1 if errors exist */
        if (dm1.dtc_count > 0) {
            uint8_t can_data[8];
            lq_j1939_format_dm1(&dm1, can_data, sizeof(can_data));
            
            /* Build J1939 identifier for DM1 (PGN 65226) */
            uint32_t can_id = lq_j1939_build_id_from_pgn(
                J1939_PGN_DM1,  /* PGN 65226 */
                6,              /* Priority 6 */
                0x28            /* Source address: Engine #1 */
            );
            
            /* Transmit on CAN bus */
            CAN_TxHeaderTypeDef tx_header = {
                .ExtId = can_id,
                .IDE = CAN_ID_EXT,
                .RTR = CAN_RTR_DATA,
                .DLC = 8
            };
            
            uint32_t mailbox;
            HAL_CAN_AddTxMessage(&hcan1, &tx_header, can_data, &mailbox);
        }
        
        vTaskDelayUntil(&last_wake, period);
    }
}

/* System initialization */
void system_init(void)
{
    /* STM32 HAL initialization */
    HAL_Init();
    SystemClock_Config();
    
    /* Initialize peripherals (CubeMX generated) */
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_CAN1_Init();
    
    /* Initialize FreeRTOS platform */
    if (lq_platform_init() != 0) {
        Error_Handler();
    }
    
    /* Initialize layered queue system */
    if (lq_generated_init() != 0) {
        Error_Handler();
    }
    
    /* Start platform-specific peripherals (ADC DMA, CAN filters, etc.) */
    lq_platform_peripherals_init();
}

/* Create application tasks */
void create_tasks(void)
{
    /* Processing task - high priority for real-time sensor fusion */
    BaseType_t result = xTaskCreate(
        lq_processing_task,
        "lq_proc",
        512,  /* Stack size in words */
        NULL,
        tskIDLE_PRIORITY + 3,
        &processing_task_handle
    );
    
    if (result != pdPASS) {
        Error_Handler();
    }
    
    /* Diagnostics task - lower priority */
    result = xTaskCreate(
        lq_diagnostics_task,
        "lq_diag",
        256,
        NULL,
        tskIDLE_PRIORITY + 2,
        &diagnostics_task_handle
    );
    
    if (result != pdPASS) {
        Error_Handler();
    }
    
    /* Cyclic output tasks are created automatically by the system */
}

/* Main entry point */
int main(void)
{
    /* Initialize system */
    system_init();
    
    /* Create tasks */
    create_tasks();
    
    /* Start FreeRTOS scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
    while (1) {
        Error_Handler();
    }
}

/* FreeRTOS hooks */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    
    /* Stack overflow detected - halt */
    __disable_irq();
    while (1);
}

void vApplicationMallocFailedHook(void)
{
    /* Heap allocation failed - halt */
    __disable_irq();
    while (1);
}

void vApplicationIdleHook(void)
{
    /* Optional: Enter low-power mode in idle task */
    /* __WFI(); */
}

/* HAL callbacks - called from ISR context */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        /* Read ADC values (multiple channels with DMA) */
        uint16_t adc_values[4];
        /* Assuming DMA fills adc_values[] */
        
        /* Push to layered queue - ISR safe */
        lq_hw_push(0, (int32_t)adc_values[0]);  /* RPM sensor 1 */
        lq_hw_push(1, (int32_t)adc_values[1]);  /* RPM sensor 2 */
        lq_hw_push(2, (int32_t)adc_values[2]);  /* Oil pressure */
        lq_hw_push(3, (int32_t)adc_values[3]);  /* Coolant temp */
        
        /* Signal processing task - uses FromISR */
        lq_platform_signal_event_isr();
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];
        
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
            /* Extract J1939 PGN */
            uint32_t pgn = (rx_header.ExtId >> 8) & 0x3FFFF;
            
            if (pgn == J1939_PGN_EEC1) {  /* Engine speed from transmission ECU */
                /* Extract RPM (bytes 0-1, little-endian, 0.125 RPM/bit) */
                uint16_t rpm_raw = (rx_data[1] << 8) | rx_data[0];
                int32_t rpm = (int32_t)(rpm_raw * 0.125f);
                
                lq_hw_push(10, rpm);  /* Signal ID 10 = RPM from CAN */
                lq_platform_signal_event_isr();
            }
        }
    }
}

/* Weak CubeMX stubs (implement these in your CubeMX generated code) */
__weak void SystemClock_Config(void) {}
__weak void MX_GPIO_Init(void) {}
__weak void MX_ADC1_Init(void) {}
__weak void MX_CAN1_Init(void) {}
__weak void Error_Handler(void) { while(1); }
