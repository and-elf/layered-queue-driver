/*
 * AUTO-GENERATED PLATFORM-SPECIFIC CODE
 * Platform: ESP32
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * This file contains real hardware ISRs and peripheral configuration
 * for flashing directly to ESP32 hardware.
 */

/* ESP32 IDF Platform Headers */
#include "driver/adc.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lq_platform.h"
#include "lq_hw_input.h"

/* SPI device handle */
static spi_device_handle_t spi_handle;

/* ========================================
 * Interrupt Service Routines
 * ======================================== */


/* ADC read for rpm_adc */
void lq_adc_read_rpm_adc(void)
{
    int value = adc1_get_raw(ADC1_CHANNEL_0);
    lq_hw_push(0, (uint32_t)value);
}


/* ADC read for temp_adc */
void lq_adc_read_temp_adc(void)
{
    int value = adc1_get_raw(ADC1_CHANNEL_1);
    lq_hw_push(2, (uint32_t)value);
}


/* ADC read for oil_adc */
void lq_adc_read_oil_adc(void)
{
    int value = adc1_get_raw(ADC1_CHANNEL_2);
    lq_hw_push(3, (uint32_t)value);
}


/* SPI read for rpm_spi */
void lq_spi_read_rpm_spi(void)
{
    spi_transaction_t trans = {
        .length = 16,
        .rxlength = 16,
    };
    
    uint16_t rx_data;
    trans.rx_buffer = &rx_data;
    
    spi_device_transmit(spi_handle, &trans);
    lq_hw_push(1, (uint32_t)rx_data);
}

/* ========================================
 * Peripheral Initialization
 * ======================================== */


/* ESP32 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* ADC Configuration */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_11);

    /* SPI Configuration */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = 5,
        .queue_size = 1,
    };
    
    spi_bus_initialize(HSPI_HOST, &bus_cfg, 1);
    spi_bus_add_device(HSPI_HOST, &dev_cfg, &spi_handle);
}
