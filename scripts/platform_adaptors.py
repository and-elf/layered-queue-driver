#!/usr/bin/env python3
"""
Platform Adaptor Layer for Layered Queue Driver

Generates platform-specific ISRs and peripheral configurations
for flashing directly to embedded hardware.

Supported platforms:
- STM32 (HAL)
- Atmel SAMD (ASF4)
- ESP32
- NRF52
- Generic bare-metal

Each adaptor generates:
- Real interrupt handlers mapped to vector table
- Peripheral initialization (ADC, SPI, timers)
- Hardware-specific configuration
"""

class PlatformAdaptor:
    """Base class for platform adaptors"""
    
    def __init__(self, platform_name):
        self.platform_name = platform_name
    
    def generate_isr_wrapper(self, node, signal_id):
        """Generate platform-specific ISR wrapper"""
        raise NotImplementedError("Subclass must implement generate_isr_wrapper")
    
    def generate_peripheral_init(self, nodes):
        """Generate peripheral initialization code"""
        raise NotImplementedError("Subclass must implement generate_peripheral_init")
    
    def generate_platform_header(self):
        """Generate platform-specific header includes"""
        raise NotImplementedError("Subclass must implement generate_platform_header")


class STM32Adaptor(PlatformAdaptor):
    """STM32 HAL adaptor - generates real interrupt handlers"""
    
    def __init__(self):
        super().__init__("STM32")
        self.adc_channels = {}
        self.spi_instances = {}
    
    def generate_platform_header(self):
        return """/* STM32 HAL Platform Headers */
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
"""
    
    def generate_isr_wrapper(self, node, signal_id):
        """Generate STM32 HAL interrupt handler"""
        if node.compatible == 'lq,hw-adc-input':
            adc_channel = node.properties.get('hw_channel', 0)
            adc_instance = node.properties.get('hw_instance', 1)
            
            return f"""
/* ADC DMA Conversion Complete Callback for {node.label} */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{{
    if (hadc->Instance == ADC{adc_instance}) {{
        uint16_t value = HAL_ADC_GetValue(&hadc{adc_instance});
        lq_hw_push({signal_id}, (uint32_t)value);
    }}
}}

/* Alternative: Polling-based ADC read */
void lq_adc_read_{node.label}(void)
{{
    HAL_ADC_Start(&hadc{adc_instance});
    if (HAL_ADC_PollForConversion(&hadc{adc_instance}, 1) == HAL_OK) {{
        uint16_t value = HAL_ADC_GetValue(&hadc{adc_instance});
        lq_hw_push({signal_id}, (uint32_t)value);
    }}
}}
"""
        
        elif node.compatible == 'lq,hw-spi-input':
            spi_instance = node.properties.get('hw_instance', 1)
            
            return f"""
/* SPI Receive Complete Callback for {node.label} */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{{
    if (hspi->Instance == SPI{spi_instance}) {{
        /* Assuming 16-bit data in spi_rx_buffer */
        extern uint16_t spi_rx_buffer;
        lq_hw_push({signal_id}, (uint32_t)spi_rx_buffer);
    }}
}}
"""
        return ""
    
    def generate_peripheral_init(self, hw_inputs):
        """Generate STM32 peripheral initialization"""
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        spi_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-spi-input']
        
        code = """
/* STM32 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* Note: This assumes CubeMX has generated HAL_ADC_MspInit, HAL_SPI_MspInit, etc. */
    
"""
        
        # ADC initialization
        if adc_nodes:
            code += "    /* ADC Configuration */\n"
            for node in adc_nodes:
                adc_instance = node.properties.get('hw_instance', 1)
                code += f"    HAL_ADC_Start_DMA(&hadc{adc_instance}, (uint32_t*)&adc_buffer, 1);\n"
        
        # SPI initialization
        if spi_nodes:
            code += "\n    /* SPI Configuration */\n"
            for node in spi_nodes:
                spi_instance = node.properties.get('hw_instance', 1)
                code += f"    HAL_SPI_Receive_IT(&hspi{spi_instance}, (uint8_t*)&spi_rx_buffer, 2);\n"
        
        code += "}\n"
        return code


class SAMDAdaptor(PlatformAdaptor):
    """Atmel SAMD ASF4 adaptor"""
    
    def __init__(self):
        super().__init__("SAMD")
    
    def generate_platform_header(self):
        return """/* Atmel SAMD ASF4 Platform Headers */
#include "atmel_start.h"
#include "hal_adc_sync.h"
#include "hal_spi_m_sync.h"
#include "lq_platform.h"
#include "lq_hw_input.h"

/* ADC and SPI descriptors from Atmel START */
extern struct adc_sync_descriptor ADC_0;
extern struct spi_m_sync_descriptor SPI_0;
"""
    
    def generate_isr_wrapper(self, node, signal_id):
        if node.compatible == 'lq,hw-adc-input':
            channel = node.properties.get('hw_channel', 0)
            
            return f"""
/* ADC read for {node.label} on channel {channel} */
void lq_adc_read_{node.label}(void)
{{
    uint8_t buffer[2];
    adc_sync_set_channel_gain(&ADC_0, {channel}, ADC_GAIN_1);
    adc_sync_set_inputs(&ADC_0, {channel}, ADC_MUXNEG_GND, {channel});
    adc_sync_enable_channel(&ADC_0, {channel});
    
    adc_sync_read_channel(&ADC_0, {channel}, buffer, 2);
    uint16_t value = (buffer[1] << 8) | buffer[0];
    
    lq_hw_push({signal_id}, (uint32_t)value);
}}
"""
        
        elif node.compatible == 'lq,hw-spi-input':
            return f"""
/* SPI read for {node.label} */
void lq_spi_read_{node.label}(void)
{{
    uint8_t rx_buffer[2];
    struct spi_xfer xfer;
    xfer.rxbuf = rx_buffer;
    xfer.txbuf = NULL;
    xfer.size = 2;
    
    spi_m_sync_transfer(&SPI_0, &xfer);
    
    int32_t value = (rx_buffer[1] << 8) | rx_buffer[0];
    lq_hw_push({signal_id}, (uint32_t)value);
}}
"""
        return ""
    
    def generate_peripheral_init(self, hw_inputs):
        return """
/* SAMD Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* ADC and SPI already initialized by atmel_start_init() */
    /* Just enable channels as needed */
    adc_sync_enable_channel(&ADC_0, 0);
}
"""


class ESP32Adaptor(PlatformAdaptor):
    """ESP32 IDF adaptor"""
    
    def __init__(self):
        super().__init__("ESP32")
    
    def generate_platform_header(self):
        return """/* ESP32 IDF Platform Headers */
#include "driver/adc.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lq_platform.h"
#include "lq_hw_input.h"

/* SPI device handle */
static spi_device_handle_t spi_handle;
"""
    
    def generate_isr_wrapper(self, node, signal_id):
        if node.compatible == 'lq,hw-adc-input':
            channel = node.properties.get('hw_channel', 0)
            
            return f"""
/* ADC read for {node.label} */
void lq_adc_read_{node.label}(void)
{{
    int value = adc1_get_raw(ADC1_CHANNEL_{channel});
    lq_hw_push({signal_id}, (uint32_t)value);
}}
"""
        
        elif node.compatible == 'lq,hw-spi-input':
            return f"""
/* SPI read for {node.label} */
void lq_spi_read_{node.label}(void)
{{
    spi_transaction_t trans = {{
        .length = 16,
        .rxlength = 16,
    }};
    
    uint16_t rx_data;
    trans.rx_buffer = &rx_data;
    
    spi_device_transmit(spi_handle, &trans);
    lq_hw_push({signal_id}, (uint32_t)rx_data);
}}
"""
        return ""
    
    def generate_peripheral_init(self, hw_inputs):
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        
        code = """
/* ESP32 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
"""
        
        if adc_nodes:
            code += """    /* ADC Configuration */
    adc1_config_width(ADC_WIDTH_BIT_12);
"""
            for node in adc_nodes:
                channel = node.properties.get('hw_channel', 0)
                code += f"    adc1_config_channel_atten(ADC1_CHANNEL_{channel}, ADC_ATTEN_DB_11);\n"
        
        code += """
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
"""
        return code


class NRF52Adaptor(PlatformAdaptor):
    """Nordic NRF52 SDK adaptor"""
    
    def __init__(self):
        super().__init__("NRF52")
    
    def generate_platform_header(self):
        return """/* Nordic NRF52 SDK Platform Headers */
#include "nrf_drv_saadc.h"
#include "nrf_drv_spi.h"
#include "lq_platform.h"
#include "lq_hw_input.h"

/* SAADC instance */
static nrf_saadc_value_t adc_buffer[8];

/* SPI instance */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);
"""
    
    def generate_isr_wrapper(self, node, signal_id):
        if node.compatible == 'lq,hw-adc-input':
            channel = node.properties.get('hw_channel', 0)
            
            return f"""
/* SAADC callback for {node.label} */
void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE) {{
        uint16_t value = p_event->data.done.p_buffer[{channel}];
        lq_hw_push({signal_id}, (uint32_t)value);
        
        /* Re-trigger conversion */
        nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 1);
    }}
}}
"""
        
        elif node.compatible == 'lq,hw-spi-input':
            return f"""
/* SPI event handler for {node.label} */
void spi_event_handler(nrf_drv_spi_evt_t const * p_event, void * p_context)
{{
    if (p_event->type == NRF_DRV_SPI_EVENT_DONE) {{
        uint16_t value = (p_event->data.done.p_rx_buffer[0] << 8) | 
                         p_event->data.done.p_rx_buffer[1];
        lq_hw_push({signal_id}, (uint32_t)value);
    }}
}}
"""
        return ""
    
    def generate_peripheral_init(self, hw_inputs):
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        
        code = """
/* NRF52 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
"""
        
        if adc_nodes:
            code += """    /* SAADC Configuration */
    ret_code_t err_code;
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);
    
    err_code = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);
    
"""
            for i, node in enumerate(adc_nodes):
                channel = node.properties.get('hw_channel', 0)
                code += f"    channel_config.pin_p = NRF_SAADC_INPUT_AIN{channel};\n"
                code += f"    err_code = nrf_drv_saadc_channel_init({i}, &channel_config);\n"
                code += f"    APP_ERROR_CHECK(err_code);\n\n"
            
            code += """    err_code = nrf_drv_saadc_buffer_convert(adc_buffer, 1);
    APP_ERROR_CHECK(err_code);
"""
        
        code += """
    /* SPI Configuration */
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = 4;
    spi_config.miso_pin = 28;
    spi_config.mosi_pin = 29;
    spi_config.sck_pin  = 3;
    
    err_code = nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL);
    APP_ERROR_CHECK(err_code);
}
"""
        return code


class BaremetalAdaptor(PlatformAdaptor):
    """Generic bare-metal adaptor with register access"""
    
    def __init__(self):
        super().__init__("Baremetal")
    
    def generate_platform_header(self):
        return """/* Generic Bare-metal Platform */
#include <stdint.h>
#include "lq_platform.h"
#include "lq_hw_input.h"

/* Define your peripheral base addresses */
#define ADC_BASE    0x40012000
#define SPI_BASE    0x40013000

/* ADC registers (example) */
#define ADC_DR      (*(volatile uint32_t*)(ADC_BASE + 0x4C))
#define ADC_SR      (*(volatile uint32_t*)(ADC_BASE + 0x00))
#define ADC_CR1     (*(volatile uint32_t*)(ADC_BASE + 0x04))
#define ADC_CR2     (*(volatile uint32_t*)(ADC_BASE + 0x08))

/* SPI registers (example) */
#define SPI_DR      (*(volatile uint32_t*)(SPI_BASE + 0x0C))
#define SPI_SR      (*(volatile uint32_t*)(SPI_BASE + 0x08))
"""
    
    def generate_isr_wrapper(self, node, signal_id):
        if node.compatible == 'lq,hw-adc-input':
            return f"""
/* ADC IRQ Handler for {node.label} */
void ADC_IRQHandler(void)
{{
    if (ADC_SR & 0x02) {{  /* EOC - End of Conversion */
        uint16_t value = (uint16_t)ADC_DR;
        lq_hw_push({signal_id}, (uint32_t)value);
    }}
}}
"""
        
        elif node.compatible == 'lq,hw-spi-input':
            return f"""
/* SPI IRQ Handler for {node.label} */
void SPI_IRQHandler(void)
{{
    if (SPI_SR & 0x01) {{  /* RXNE - Receive buffer not empty */
        uint16_t value = (uint16_t)SPI_DR;
        lq_hw_push({signal_id}, (uint32_t)value);
    }}
}}
"""
        return ""
    
    def generate_peripheral_init(self, hw_inputs):
        return """
/* Bare-metal Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* Enable peripheral clocks */
    /* Configure GPIO pins */
    /* Configure ADC */
    ADC_CR1 = 0x00000100;  /* SCAN mode */
    ADC_CR2 = 0x00000001;  /* ADON - ADC ON */
    
    /* Configure SPI */
    SPI_CR1 = 0x0304;  /* Master mode, CPOL=0, CPHA=0 */
    
    /* Enable interrupts in NVIC */
}
"""


def get_platform_adaptor(platform):
    """Factory function to get the appropriate platform adaptor"""
    adaptors = {
        'stm32': STM32Adaptor,
        'samd': SAMDAdaptor,
        'esp32': ESP32Adaptor,
        'nrf52': NRF52Adaptor,
        'baremetal': BaremetalAdaptor,
    }
    
    adaptor_class = adaptors.get(platform.lower())
    if not adaptor_class:
        raise ValueError(f"Unknown platform: {platform}. Supported: {list(adaptors.keys())}")
    
    return adaptor_class()
