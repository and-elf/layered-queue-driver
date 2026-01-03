/*
 * AUTO-GENERATED PLATFORM-SPECIFIC CODE
 * Platform: NRF52
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * This file contains real hardware ISRs and peripheral configuration
 * for flashing directly to NRF52 hardware.
 */

/* Nordic NRF52 SDK Platform Headers */
#include "nrf_drv_saadc.h"
#include "nrf_drv_spi.h"
#include "lq_platform.h"
#include "lq_hw_input.h"

/* SAADC instance */
static nrf_saadc_value_t adc_buffer[8];

/* SPI instance */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);

/* ========================================
 * Interrupt Service Routines
 * ======================================== */


/* SAADC callback for rpm_adc */
void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE) {
        uint16_t value = p_event->data.done.p_buffer[0];
        lq_hw_push(0, (uint32_t)value);
        
        /* Re-trigger conversion */
        nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 1);
    }
}


/* SAADC callback for temp_adc */
void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE) {
        uint16_t value = p_event->data.done.p_buffer[1];
        lq_hw_push(2, (uint32_t)value);
        
        /* Re-trigger conversion */
        nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 1);
    }
}


/* SAADC callback for oil_adc */
void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE) {
        uint16_t value = p_event->data.done.p_buffer[2];
        lq_hw_push(3, (uint32_t)value);
        
        /* Re-trigger conversion */
        nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 1);
    }
}


/* SPI event handler for rpm_spi */
void spi_event_handler(nrf_drv_spi_evt_t const * p_event, void * p_context)
{
    if (p_event->type == NRF_DRV_SPI_EVENT_DONE) {
        uint16_t value = (p_event->data.done.p_rx_buffer[0] << 8) | 
                         p_event->data.done.p_rx_buffer[1];
        lq_hw_push(1, (uint32_t)value);
    }
}

/* ========================================
 * Peripheral Initialization
 * ======================================== */


/* NRF52 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* SAADC Configuration */
    ret_code_t err_code;
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);
    
    err_code = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);
    
    channel_config.pin_p = NRF_SAADC_INPUT_AIN0;
    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);

    channel_config.pin_p = NRF_SAADC_INPUT_AIN1;
    err_code = nrf_drv_saadc_channel_init(1, &channel_config);
    APP_ERROR_CHECK(err_code);

    channel_config.pin_p = NRF_SAADC_INPUT_AIN2;
    err_code = nrf_drv_saadc_channel_init(2, &channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(adc_buffer, 1);
    APP_ERROR_CHECK(err_code);

    /* SPI Configuration */
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = 4;
    spi_config.miso_pin = 28;
    spi_config.mosi_pin = 29;
    spi_config.sck_pin  = 3;
    
    err_code = nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL);
    APP_ERROR_CHECK(err_code);
}
