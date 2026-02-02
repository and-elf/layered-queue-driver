"""STM32 HAL platform generator."""

from typing import List, Any
from .base import PlatformGenerator


class STM32Generator(PlatformGenerator):
    """
    STM32 HAL platform generator.
    
    Generates real interrupt handlers for STM32 microcontrollers using
    the STM32 HAL (Hardware Abstraction Layer). Works with CubeMX-generated
    projects.
    
    Supported peripherals:
    - ADC (with DMA and polling modes)
    - SPI (interrupt-based)
    - CAN (bxCAN and FDCAN)
    - GPIO
    - I2C
    - UART
    """
    
    def __init__(self):
        super().__init__("STM32 HAL")
    
    def generate_platform_header(self) -> str:
        """STM32 HAL headers and extern handles."""
        return """/* STM32 HAL Platform Headers */
#include "stm32f4xx_hal.h"  /* Adjust for your STM32 family */
#include "lq_platform.h"
#include "lq_hw_input.h"
#include "lq_generated.h"

/* ADC handles (configured by CubeMX or manually) */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;

/* SPI handles */
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

/* CAN handles - Uses STM32's BUILT-IN CAN controller (bxCAN or FDCAN)
 * You only need an external CAN transceiver chip (TJA1050, MCP2551, etc.)
 * to convert TX/RX logic levels to differential CANH/CANL bus signals.
 */
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

/* I2C handles */
extern I2C_HandleTypeDef hi2c1;

/* UART handles */
extern UART_HandleTypeDef huart2;

/* DMA handles if using DMA */
extern DMA_HandleTypeDef hdma_adc1;

/* Buffers for interrupt-based peripherals */
static uint16_t adc_buffer;
static uint16_t spi_rx_buffer;
static uint8_t uart_rx_buffer;

"""
    
    def generate_isr_wrappers(self, nodes: List[Any]) -> str:
        """Generate STM32 HAL interrupt callbacks."""
        hw_inputs = [n for n in nodes if n.compatible.startswith('lq,hw-')]
        
        code = """/* ========================================
 * Interrupt Service Routines
 * ======================================== */

"""
        
        for node in hw_inputs:
            signal_id = node.signal_id or node.properties.get('signal_id', 0)
            isr = self._generate_node_isr(node, signal_id)
            if isr:
                code += isr + "\n"
        
        return code
    
    def _generate_node_isr(self, node: Any, signal_id: int) -> str:
        """Generate ISR wrapper for a specific node."""
        if node.compatible == 'lq,hw-adc-input':
            adc_channel = node.properties.get('hw_channel', 0)
            adc_instance = node.properties.get('hw_instance', 1)
            
            return f"""/* ADC DMA Conversion Complete Callback for {node.label} */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{{
    if (hadc->Instance == ADC{adc_instance}) {{
        uint16_t value = HAL_ADC_GetValue(&hadc{adc_instance});
        lq_adc_isr_{node.label}(value);
    }}
}}

/* Alternative: Polling-based ADC read */
void lq_adc_read_{node.label}(void)
{{
    HAL_ADC_Start(&hadc{adc_instance});
    if (HAL_ADC_PollForConversion(&hadc{adc_instance}, 1) == HAL_OK) {{
        uint16_t value = HAL_ADC_GetValue(&hadc{adc_instance});
        lq_adc_isr_{node.label}(value);
    }}
}}
"""
        
        elif node.compatible == 'lq,hw-spi-input':
            spi_instance = node.properties.get('hw_instance', 1)
            
            return f"""/* SPI Receive Complete Callback for {node.label} */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{{
    if (hspi->Instance == SPI{spi_instance}) {{
        lq_spi_isr_{node.label}((int32_t)spi_rx_buffer);
        /* Restart SPI reception */
        HAL_SPI_Receive_IT(&hspi{spi_instance}, (uint8_t*)&spi_rx_buffer, 2);
    }}
}}
"""
        
        elif node.compatible == 'lq,hw-can-input':
            can_instance = node.properties.get('hw_instance', 1)
            pgn = node.properties.get('pgn', 0)
            
            return f"""/* CAN Receive Callback for {node.label} (PGN 0x{pgn:04X}) */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{{
    if (hcan->Instance == CAN{can_instance}) {{
        CAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];
        
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {{
            /* Extract J1939 PGN from 29-bit identifier */
            uint32_t id = rx_header.ExtId;
            uint32_t msg_pgn = (id >> 8) & 0x3FFFF;
            
            if (msg_pgn == 0x{pgn:04X}) {{
                /* Convert CAN data to int32_t (little-endian) */
                int32_t value = (rx_data[3] << 24) | (rx_data[2] << 16) | 
                                (rx_data[1] << 8) | rx_data[0];
                /* Call generic ISR handler from lq_generated.c */
                lq_hw_push({signal_id}, value);
            }}
        }}
    }}
}}
"""
        
        return ""
    
    def generate_peripheral_init(self, nodes: List[Any]) -> str:
        """Generate STM32 peripheral initialization."""
        hw_inputs = [n for n in nodes if n.compatible.startswith('lq,hw-')]
        
        # Categorize nodes by peripheral type
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        spi_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-spi-input']
        can_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-can-input']
        gpio_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-gpio-input']
        i2c_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-i2c-input']
        uart_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-uart-input']
        
        code = """/* ========================================
 * Peripheral Initialization
 * ======================================== */

/* STM32 Peripheral Initialization
 * 
 * Note: This assumes CubeMX has generated HAL_ADC_MspInit, HAL_SPI_MspInit, etc.
 * This function only starts the peripherals and configures filters/interrupts.
 */
void lq_platform_peripherals_init(void)
{
"""
        
        # GPIO initialization
        if gpio_nodes:
            code += "    /* GPIO Configuration */\n"
            for node in gpio_nodes:
                gpio_port = node.properties.get('hw_port', 'A')
                gpio_pin = node.properties.get('hw_pin', 0)
                gpio_mode = 'INPUT'
                code += f"""    /* Configure GPIO{gpio_port}{gpio_pin} as {gpio_mode} for {node.label} */
    GPIO_InitTypeDef GPIO_InitStruct = {{0}};
    GPIO_InitStruct.Pin = GPIO_PIN_{gpio_pin};
    GPIO_InitStruct.Mode = GPIO_MODE_{gpio_mode};
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIO{gpio_port}, &GPIO_InitStruct);
    
"""
        
        # ADC initialization
        if adc_nodes:
            code += "    /* ADC Configuration */\n"
            for node in adc_nodes:
                adc_instance = node.properties.get('hw_instance', 1)
                code += f"    HAL_ADC_Start_DMA(&hadc{adc_instance}, (uint32_t*)&adc_buffer, 1);\n"
            code += "\n"
        
        # I2C initialization
        if i2c_nodes:
            code += "    /* I2C Configuration */\n"
            for node in i2c_nodes:
                i2c_instance = node.properties.get('hw_instance', 1)
                i2c_addr = node.properties.get('i2c_address', 0x50)
                code += f"    /* I2C{i2c_instance} ready for device at address 0x{i2c_addr:02X} ({node.label}) */\n"
            code += "\n"
        
        # SPI initialization
        if spi_nodes:
            code += "    /* SPI Configuration */\n"
            for node in spi_nodes:
                spi_instance = node.properties.get('hw_instance', 1)
                code += f"    HAL_SPI_Receive_IT(&hspi{spi_instance}, (uint8_t*)&spi_rx_buffer, 2);\n"
            code += "\n"
        
        # UART initialization
        if uart_nodes:
            code += "    /* UART Configuration */\n"
            for node in uart_nodes:
                uart_instance = node.properties.get('hw_instance', 2)
                code += f"    HAL_UART_Receive_IT(&huart{uart_instance}, (uint8_t*)&uart_rx_buffer, 1);\n"
            code += "\n"
        
        # CAN initialization
        if can_nodes:
            code += "    /* CAN Configuration */\n"
            for node in can_nodes:
                can_instance = node.properties.get('hw_instance', 1)
                pgn = node.properties.get('pgn', 0)
                code += f"""    /* Configure CAN filter for PGN 0x{pgn:04X} ({node.label}) */
    CAN_FilterTypeDef can_filter;
    can_filter.FilterIdHigh = (0x{pgn:04X} << 8) >> 16;
    can_filter.FilterIdLow = (0x{pgn:04X} << 8) & 0xFFFF;
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
        
        code += """    /* All peripherals initialized */
}
"""
        return code
    
    def _main_loop_body(self) -> str:
        """STM32 main loop - use WFI for power efficiency."""
        return """/* Wait for interrupt (power-efficient sleep) */
        __WFI();"""
