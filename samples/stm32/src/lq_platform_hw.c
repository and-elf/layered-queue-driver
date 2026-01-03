/*
 * AUTO-GENERATED PLATFORM-SPECIFIC CODE
 * Platform: STM32
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * This file contains real hardware ISRs and peripheral configuration
 * for flashing directly to STM32 hardware.
 */

/* STM32 HAL Platform Headers */
#include "stm32f4xx_hal.h"  /* Adjust for your STM32 family */
#include "lq_platform.h"
#include "lq_hw_input.h"

/* ADC handles (configured by CubeMX or manually) */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;

/* SPI handles */
extern SPI_HandleTypeDef hspi1;

/* CAN handles - Uses STM32's BUILT-IN CAN controller (bxCAN or FDCAN)
 * You only need an external CAN transceiver chip (TJA1050, MCP2551, etc.)
 * to convert TX/RX logic levels to differential CANH/CANL bus signals.
 */
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

/* DMA handles if using DMA */
extern DMA_HandleTypeDef hdma_adc1;

/* ========================================
 * Interrupt Service Routines
 * ======================================== */


/* ADC DMA Conversion Complete Callback for rpm_adc_1 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(0, (uint32_t)value);
    }
}

/* Alternative: Polling-based ADC read */
void lq_adc_read_rpm_adc_1(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(0, (uint32_t)value);
    }
}


/* ADC DMA Conversion Complete Callback for rpm_adc_2 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(1, (uint32_t)value);
    }
}

/* Alternative: Polling-based ADC read */
void lq_adc_read_rpm_adc_2(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(1, (uint32_t)value);
    }
}


/* ADC DMA Conversion Complete Callback for oil_pressure_adc */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(2, (uint32_t)value);
    }
}

/* Alternative: Polling-based ADC read */
void lq_adc_read_oil_pressure_adc(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(2, (uint32_t)value);
    }
}


/* ADC DMA Conversion Complete Callback for coolant_temp_adc */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(3, (uint32_t)value);
    }
}

/* Alternative: Polling-based ADC read */
void lq_adc_read_coolant_temp_adc(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(3, (uint32_t)value);
    }
}


/* CAN Receive Callback for rpm_can (PGN 65265) */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];
        
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
            /* Extract J1939 PGN from 29-bit identifier */
            uint32_t id = rx_header.ExtId;
            uint32_t msg_pgn = (id >> 8) & 0x3FFFF;
            
            if (msg_pgn == 65265) {
                /* Convert CAN data to int32_t (platform-specific format) */
                int32_t value = (rx_data[3] << 24) | (rx_data[2] << 16) | 
                                (rx_data[1] << 8) | rx_data[0];
                lq_hw_push(10, value);
            }
        }
    }
}


/* CAN Receive Callback for vehicle_speed_can (PGN 65265) */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];
        
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
            /* Extract J1939 PGN from 29-bit identifier */
            uint32_t id = rx_header.ExtId;
            uint32_t msg_pgn = (id >> 8) & 0x3FFFF;
            
            if (msg_pgn == 65265) {
                /* Convert CAN data to int32_t (platform-specific format) */
                int32_t value = (rx_data[3] << 24) | (rx_data[2] << 16) | 
                                (rx_data[1] << 8) | rx_data[0];
                lq_hw_push(11, value);
            }
        }
    }
}


/* CAN Receive Callback for fuel_rate_can (PGN 65266) */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];
        
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
            /* Extract J1939 PGN from 29-bit identifier */
            uint32_t id = rx_header.ExtId;
            uint32_t msg_pgn = (id >> 8) & 0x3FFFF;
            
            if (msg_pgn == 65266) {
                /* Convert CAN data to int32_t (platform-specific format) */
                int32_t value = (rx_data[3] << 24) | (rx_data[2] << 16) | 
                                (rx_data[1] << 8) | rx_data[0];
                lq_hw_push(12, value);
            }
        }
    }
}

/* ========================================
 * Peripheral Initialization
 * ======================================== */


/* STM32 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* Note: This assumes CubeMX has generated HAL_ADC_MspInit, HAL_SPI_MspInit, etc. */
    
    /* ADC Configuration */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&adc_buffer, 1);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&adc_buffer, 1);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&adc_buffer, 1);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&adc_buffer, 1);

    /* CAN Configuration */
    /* Configure CAN filter for PGN 65265 */
    CAN_FilterTypeDef can_filter;
    can_filter.FilterIdHigh = (65265 << 8) >> 16;
    can_filter.FilterIdLow = (65265 << 8) & 0xFFFF;
    can_filter.FilterMaskIdHigh = 0xFFFF;
    can_filter.FilterMaskIdLow = 0xFFFF;
    can_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    can_filter.FilterBank = 0;
    can_filter.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan1, &can_filter);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    /* Configure CAN filter for PGN 65265 */
    CAN_FilterTypeDef can_filter;
    can_filter.FilterIdHigh = (65265 << 8) >> 16;
    can_filter.FilterIdLow = (65265 << 8) & 0xFFFF;
    can_filter.FilterMaskIdHigh = 0xFFFF;
    can_filter.FilterMaskIdLow = 0xFFFF;
    can_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    can_filter.FilterBank = 0;
    can_filter.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan1, &can_filter);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    /* Configure CAN filter for PGN 65266 */
    CAN_FilterTypeDef can_filter;
    can_filter.FilterIdHigh = (65266 << 8) >> 16;
    can_filter.FilterIdLow = (65266 << 8) & 0xFFFF;
    can_filter.FilterMaskIdHigh = 0xFFFF;
    can_filter.FilterMaskIdLow = 0xFFFF;
    can_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    can_filter.FilterBank = 0;
    can_filter.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan1, &can_filter);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}
