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

/* DMA handles if using DMA */
extern DMA_HandleTypeDef hdma_adc1;

/* ========================================
 * Interrupt Service Routines
 * ======================================== */


/* ADC DMA Conversion Complete Callback for rpm_adc */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(0, (uint32_t)value);
    }
}

/* Alternative: Polling-based ADC read */
void lq_adc_read_rpm_adc(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(0, (uint32_t)value);
    }
}


/* ADC DMA Conversion Complete Callback for temp_adc */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(2, (uint32_t)value);
    }
}

/* Alternative: Polling-based ADC read */
void lq_adc_read_temp_adc(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(2, (uint32_t)value);
    }
}


/* ADC DMA Conversion Complete Callback for oil_adc */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(3, (uint32_t)value);
    }
}

/* Alternative: Polling-based ADC read */
void lq_adc_read_oil_adc(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(3, (uint32_t)value);
    }
}


/* SPI Receive Complete Callback for rpm_spi */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        /* Assuming 16-bit data in spi_rx_buffer */
        extern uint16_t spi_rx_buffer;
        lq_hw_push(1, (uint32_t)spi_rx_buffer);
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

    /* SPI Configuration */
    HAL_SPI_Receive_IT(&hspi1, (uint8_t*)&spi_rx_buffer, 2);
}
