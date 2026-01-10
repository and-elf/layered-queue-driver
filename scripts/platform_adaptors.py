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
- AVR

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

/* CAN handles - Uses STM32's BUILT-IN CAN controller (bxCAN or FDCAN)
 * You only need an external CAN transceiver chip (TJA1050, MCP2551, etc.)
 * to convert TX/RX logic levels to differential CANH/CANL bus signals.
 */
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

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
        
        elif node.compatible == 'lq,hw-can-input':
            can_instance = node.properties.get('hw_instance', 1)
            pgn = node.properties.get('pgn', 0)
            
            return f"""
/* CAN Receive Callback for {node.label} (PGN {pgn}) */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{{
    if (hcan->Instance == CAN{can_instance}) {{
        CAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];
        
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {{
            /* Extract J1939 PGN from 29-bit identifier */
            uint32_t id = rx_header.ExtId;
            uint32_t msg_pgn = (id >> 8) & 0x3FFFF;
            
            if (msg_pgn == {pgn}) {{
                /* Convert CAN data to int32_t (platform-specific format) */
                int32_t value = (rx_data[3] << 24) | (rx_data[2] << 16) | 
                                (rx_data[1] << 8) | rx_data[0];
                lq_hw_push({signal_id}, value);
            }}
        }}
    }}
}}
"""
        return ""
    
    def generate_peripheral_init(self, hw_inputs):
        """Generate STM32 peripheral initialization"""
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        spi_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-spi-input']
        gpio_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-gpio-input']
        i2c_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-i2c-input']
        uart_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-uart-input']
        
        code = """
/* STM32 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* Note: This assumes CubeMX has generated HAL_ADC_MspInit, HAL_SPI_MspInit, etc. */
    
"""
        
        # GPIO initialization
        if gpio_nodes:
            code += "    /* GPIO Configuration */\n"
            for node in gpio_nodes:
                gpio_port = node.properties.get('hw_port', 'A')
                gpio_pin = node.properties.get('hw_pin', 0)
                # Automatically determine direction from node type
                if node.compatible == 'lq,hw-gpio-input':
                    gpio_mode = 'INPUT'
                else:  # Output nodes (lq,gpio-output, etc.)
                    gpio_mode = 'OUTPUT_PP'
                code += f"""    /* Configure GPIO{gpio_port}{gpio_pin} as {gpio_mode} for {node.label} */
    GPIO_InitTypeDef GPIO_InitStruct = {{0}};
    GPIO_InitStruct.Pin = GPIO_PIN_{gpio_pin};
    GPIO_InitStruct.Mode = GPIO_MODE_{gpio_mode};
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIO{gpio_port}, &GPIO_InitStruct);
"""
        
        # ADC initialization
        if adc_nodes:
            code += "\n    /* ADC Configuration */\n"
            for node in adc_nodes:
                adc_instance = node.properties.get('hw_instance', 1)
                code += f"    HAL_ADC_Start_DMA(&hadc{adc_instance}, (uint32_t*)&adc_buffer, 1);\n"
        
        # I2C initialization
        if i2c_nodes:
            code += "\n    /* I2C Configuration */\n"
            for node in i2c_nodes:
                i2c_instance = node.properties.get('hw_instance', 1)
                i2c_addr = node.properties.get('i2c_address', 0x50)
                code += f"    /* I2C{i2c_instance} ready for device at address 0x{i2c_addr:02X} ({node.label}) */\n"
        
        # SPI initialization
        if spi_nodes:
            code += "\n    /* SPI Configuration */\n"
            for node in spi_nodes:
                spi_instance = node.properties.get('hw_instance', 1)
                code += f"    HAL_SPI_Receive_IT(&hspi{spi_instance}, (uint8_t*)&spi_rx_buffer, 2);\n"
        
        # UART initialization
        if uart_nodes:
            code += "\n    /* UART Configuration */\n"
            for node in uart_nodes:
                uart_instance = node.properties.get('hw_instance', 2)
                code += f"    HAL_UART_Receive_IT(&huart{uart_instance}, (uint8_t*)&uart_rx_buffer, 1);\n"
        
        # CAN initialization
        can_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-can-input']
        if can_nodes:
            code += "\n    /* CAN Configuration */\n"
            for node in can_nodes:
                can_instance = node.properties.get('hw_instance', 1)
                pgn = node.properties.get('pgn', 0)
                code += f"""    /* Configure CAN filter for PGN {pgn} */
    CAN_FilterTypeDef can_filter;
    can_filter.FilterIdHigh = ({pgn} << 8) >> 16;
    can_filter.FilterIdLow = ({pgn} << 8) & 0xFFFF;
    can_filter.FilterMaskIdHigh = 0xFFFF;
    can_filter.FilterMaskIdLow = 0xFFFF;
    can_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    can_filter.FilterBank = 0;
    can_filter.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan{can_instance}, &can_filter);
    HAL_CAN_Start(&hcan{can_instance});
    HAL_CAN_ActivateNotification(&hcan{can_instance}, CAN_IT_RX_FIFO0_MSG_PENDING);
"""
        
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
        gpio_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-gpio-input']
        i2c_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-i2c-input']
        uart_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-uart-input']
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        
        code = """
/* SAMD Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* Note: atmel_start_init() should be called before this */
    
"""
        
        # GPIO initialization
        if gpio_nodes:
            code += "    /* GPIO Configuration */\n"
            for node in gpio_nodes:
                gpio_pin = node.properties.get('hw_pin', 0)
                # Automatically determine direction from node type
                if node.compatible == 'lq,hw-gpio-input':
                    gpio_mode = 'INPUT'
                else:
                    gpio_mode = 'OUTPUT'
                code += f"    gpio_set_pin_direction({gpio_pin}, GPIO_DIRECTION_{gpio_mode});\n"
        
        # ADC initialization
        if adc_nodes:
            code += "\n    /* ADC Configuration */\n"
            for node in adc_nodes:
                channel = node.properties.get('hw_channel', 0)
                code += f"    adc_sync_enable_channel(&ADC_0, {channel});\n"
        
        # I2C initialization
        if i2c_nodes:
            code += "\n    /* I2C Configuration */\n"
            for node in i2c_nodes:
                i2c_addr = node.properties.get('i2c_address', 0x50)
                code += f"    /* I2C ready for device at address 0x{i2c_addr:02X} ({node.label}) */\n"
        
        # UART initialization
        if uart_nodes:
            code += "\n    /* UART Configuration */\n"
            code += "    usart_sync_enable(&USART_0);\n"
        
        code += "}\n"
        return code


class ESP32Adaptor(PlatformAdaptor):
    """ESP32 IDF adaptor"""
    
    def __init__(self):
        super().__init__("ESP32")
    
    def generate_platform_header(self):
        return """/* ESP32 IDF Platform Headers */
#include "driver/adc.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/twai.h"  /* Two-Wire Automotive Interface (CAN) */
#include "lq_platform.h"
#include "lq_hw_input.h"

/* SPI device handle */
static spi_device_handle_t spi_handle;

/* CAN (TWAI) configuration */
static twai_handle_t twai_handle;
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
        
        elif node.compatible == 'lq,hw-can-input':
            pgn = node.properties.get('pgn', 0)
            
            return f"""
/* CAN (TWAI) receive task for {node.label} (PGN {pgn}) */
void lq_can_receive_{node.label}(void)
{{
    twai_message_t rx_msg;
    
    if (twai_receive(&rx_msg, pdMS_TO_TICKS(0)) == ESP_OK) {{
        if (rx_msg.extd && !rx_msg.rtr) {{
            /* Extract J1939 PGN from 29-bit identifier */
            uint32_t msg_pgn = (rx_msg.identifier >> 8) & 0x3FFFF;
            
            if (msg_pgn == {pgn}) {{
                /* Convert CAN data to int32_t */
                int32_t value = (rx_msg.data[3] << 24) | (rx_msg.data[2] << 16) |
                                (rx_msg.data[1] << 8) | rx_msg.data[0];
                lq_hw_push({signal_id}, value);
            }}
        }}
    }}
}}
"""
        return ""
    
    def generate_peripheral_init(self, hw_inputs):
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        gpio_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-gpio-input']
        i2c_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-i2c-input']
        uart_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-uart-input']
        spi_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-spi-input']
        
        code = """
/* ESP32 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
"""
        
        # GPIO initialization
        if gpio_nodes:
            code += "    /* GPIO Configuration */\n"
            for node in gpio_nodes:
                gpio_pin = node.properties.get('hw_pin', 0)
                # Automatically determine direction from node type
                if node.compatible == 'lq,hw-gpio-input':
                    code += f"    gpio_set_direction(GPIO_NUM_{gpio_pin}, GPIO_MODE_INPUT);\n"
                    code += f"    gpio_set_pull_mode(GPIO_NUM_{gpio_pin}, GPIO_PULLUP_ONLY);\n"
                else:  # Output nodes
                    code += f"    gpio_set_direction(GPIO_NUM_{gpio_pin}, GPIO_MODE_OUTPUT);\n"
            code += "\n"
        
        # ADC initialization
        if adc_nodes:
            code += "    /* ADC Configuration */\n"
            code += "    adc1_config_width(ADC_WIDTH_BIT_12);\n"
            for node in adc_nodes:
                channel = node.properties.get('hw_channel', 0)
                code += f"    adc1_config_channel_atten(ADC1_CHANNEL_{channel}, ADC_ATTEN_DB_11);\n"
            code += "\n"
        
        # I2C initialization
        if i2c_nodes:
            i2c_instance = i2c_nodes[0].properties.get('hw_instance', 0)
            code += f"""    /* I2C Configuration */
    i2c_config_t i2c_conf = {{
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    }};
    i2c_param_config(I2C_NUM_{i2c_instance}, &i2c_conf);
    i2c_driver_install(I2C_NUM_{i2c_instance}, I2C_MODE_MASTER, 0, 0, 0);

"""
        
        # UART initialization
        if uart_nodes:
            uart_instance = uart_nodes[0].properties.get('hw_instance', 1)
            code += f"""    /* UART Configuration */
    uart_config_t uart_config = {{
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    }};
    uart_param_config(UART_NUM_{uart_instance}, &uart_config);
    uart_set_pin(UART_NUM_{uart_instance}, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_{uart_instance}, 256, 0, 0, NULL, 0);

"""
        
        # SPI initialization
        if spi_nodes:
            code += """    /* SPI Configuration */
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

"""
        
        # CAN initialization
        can_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-can-input']
        if can_nodes:
            code += """    /* CAN (TWAI) Configuration */
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_21, GPIO_NUM_22, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    /* Install TWAI driver */
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        twai_start();
    }
"""
        
        code += "}\n"
        return code


class NRF52Adaptor(PlatformAdaptor):
    """Nordic NRF52 SDK adaptor"""
    
    def __init__(self):
        super().__init__("NRF52")
    
    def generate_platform_header(self):
        return """/* Nordic NRF52 SDK Platform Headers */
#include "nrf_drv_saadc.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_can.h"  /* External CAN controller (MCP2515 via SPI) */
#include "lq_platform.h"
#include "lq_hw_input.h"

/* SAADC instance */
static nrf_saadc_value_t adc_buffer[8];

/* SPI instance */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);

/* CAN frame buffer */
static uint8_t can_rx_buffer[13];
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
        
        elif node.compatible == 'lq,hw-can-input':
            pgn = node.properties.get('pgn', 0)
            
            return f"""
/* CAN receive handler for {node.label} (PGN {pgn}) via MCP2515 */
void lq_can_receive_{node.label}(void)
{{
    /* Poll MCP2515 via SPI for new messages */
    if (nrf_drv_can_read_message(can_rx_buffer, sizeof(can_rx_buffer)) == NRF_SUCCESS) {{
        /* Extract J1939 identifier and PGN */
        uint32_t id = (can_rx_buffer[0] << 24) | (can_rx_buffer[1] << 16) |
                      (can_rx_buffer[2] << 8) | can_rx_buffer[3];
        uint32_t msg_pgn = (id >> 8) & 0x3FFFF;
        
        if (msg_pgn == {pgn}) {{
            /* Data starts at offset 5 in buffer */
            int32_t value = (can_rx_buffer[8] << 24) | (can_rx_buffer[7] << 16) |
                            (can_rx_buffer[6] << 8) | can_rx_buffer[5];
            lq_hw_push({signal_id}, value);
        }}
    }}
}}
"""
        return ""
    
    def generate_peripheral_init(self, hw_inputs):
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        gpio_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-gpio-input']
        i2c_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-i2c-input']
        uart_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-uart-input']
        spi_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-spi-input']
        
        code = """
/* NRF52 Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    ret_code_t err_code;
    
"""
        
        # GPIO initialization
        if gpio_nodes:
            code += "    /* GPIO Configuration */\n"
            for node in gpio_nodes:
                gpio_pin = node.properties.get('hw_pin', 0)
                # Automatically determine direction from node type
                if node.compatible == 'lq,hw-gpio-input':
                    code += f"    nrf_gpio_cfg_input({gpio_pin}, NRF_GPIO_PIN_PULLUP);\n"
                else:  # Output nodes
                    code += f"    nrf_gpio_cfg_output({gpio_pin});\n"
            code += "\n"
        
        # ADC initialization
        if adc_nodes:
            code += """    /* SAADC Configuration */
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
        
        # I2C initialization (TWI on nRF52)
        if i2c_nodes:
            i2c_instance = i2c_nodes[0].properties.get('hw_instance', 0)
            code += f"""    /* TWI (I2C) Configuration */
    nrf_drv_twi_config_t twi_config = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_config.scl = 27;
    twi_config.sda = 26;
    twi_config.frequency = NRF_DRV_TWI_FREQ_100K;
    
    static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE({i2c_instance});
    err_code = nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
    APP_ERROR_CHECK(err_code);
    nrf_drv_twi_enable(&m_twi);

"""
        
        # UART initialization
        if uart_nodes:
            uart_instance = uart_nodes[0].properties.get('hw_instance', 0)
            code += f"""    /* UART Configuration */
    nrf_drv_uart_config_t uart_config = NRF_DRV_UART_DEFAULT_CONFIG;
    uart_config.pseltxd = 6;
    uart_config.pselrxd = 8;
    uart_config.baudrate = NRF_UART_BAUDRATE_115200;
    
    static const nrf_drv_uart_t m_uart = NRF_DRV_UART_INSTANCE({uart_instance});
    err_code = nrf_drv_uart_init(&m_uart, &uart_config, NULL);
    APP_ERROR_CHECK(err_code);

"""
        
        # SPI initialization
        if spi_nodes:
            code += """    /* SPI Configuration */
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = 4;
    spi_config.miso_pin = 28;
    spi_config.mosi_pin = 29;
    spi_config.sck_pin  = 3;
    
    err_code = nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL);
    APP_ERROR_CHECK(err_code);
"""
        
        code += "}\n"
        return code


class AVRAdaptor(PlatformAdaptor):
    """AVR adaptor with direct register access"""
    
    def __init__(self):
        super().__init__("AVR")
    
    def generate_platform_header(self):
        return """/* AVR Platform */
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
        gpio_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-gpio-input']
        i2c_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-i2c-input']
        uart_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-uart-input']
        
        code = """
/* AVR Peripheral Initialization */
void lq_platform_peripherals_init(void)
{
    /* Enable peripheral clocks */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_SPI1EN | RCC_APB2ENR_USART1EN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    
"""
        
        # GPIO initialization
        if gpio_nodes:
            code += "    /* GPIO Configuration */\n"
            for node in gpio_nodes:
                gpio_port = node.properties.get('hw_port', 'A')
                gpio_pin = node.properties.get('hw_pin', 0)
                # Automatically determine direction from node type
                if node.compatible == 'lq,hw-gpio-input':
                    code += f"    GPIO{gpio_port}->CRL &= ~(0xF << ({gpio_pin}*4));\n"
                    code += f"    GPIO{gpio_port}->CRL |= (0x8 << ({gpio_pin}*4));  /* Input pull-up */\n"
                else:  # Output nodes
                    code += f"    GPIO{gpio_port}->CRL &= ~(0xF << ({gpio_pin}*4));\n"
                    code += f"    GPIO{gpio_port}->CRL |= (0x3 << ({gpio_pin}*4));  /* Output 50MHz push-pull */\n"
            code += "\n"
        
        code += """    /* Configure ADC */
    ADC_CR1 = 0x00000100;  /* SCAN mode */
    ADC_CR2 = 0x00000001;  /* ADON - ADC ON */
    
"""
        
        # I2C initialization
        if i2c_nodes:
            code += """    /* Configure I2C */
    I2C1->CR1 = 0x0000;     /* Disable I2C */
    I2C1->CR2 = 0x0024;     /* 36 MHz peripheral clock */
    I2C1->CCR = 0x00B4;     /* 100kHz standard mode */
    I2C1->TRISE = 0x0025;   /* Max rise time */
    I2C1->CR1 = 0x0001;     /* Enable I2C */
    
"""
        
        code += """    /* Configure SPI */
    SPI_CR1 = 0x0304;  /* Master mode, CPOL=0, CPHA=0 */
    
"""
        
        # UART initialization
        if uart_nodes:
            code += """    /* Configure UART */
    USART1->BRR = 0x0271;   /* 115200 baud @ 72MHz */
    USART1->CR1 = 0x200C;   /* Enable TX, RX, USART */
    
"""
        
        code += """    /* Enable interrupts in NVIC */
    NVIC_EnableIRQ(ADC_IRQn);
    NVIC_EnableIRQ(SPI1_IRQn);
}
"""
        return code


def get_platform_adaptor(platform):
    """Factory function to get the appropriate platform adaptor"""
    adaptors = {
        'stm32': STM32Adaptor,
        'samd': SAMDAdaptor,
        'esp32': ESP32Adaptor,
        'nrf52': NRF52Adaptor,
        'avr': AVRAdaptor,
    }
    
    adaptor_class = adaptors.get(platform.lower())
    if not adaptor_class:
        raise ValueError(f"Unknown platform: {platform}. Supported: {list(adaptors.keys())}")
    
    return adaptor_class()
