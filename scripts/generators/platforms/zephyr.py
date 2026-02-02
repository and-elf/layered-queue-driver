"""Zephyr RTOS platform generator with ISR-based callbacks."""

from typing import List, Any
from .base import PlatformGenerator


class ZephyrGenerator(PlatformGenerator):
    """
    Zephyr RTOS platform generator.
    
    Generates ISR-based callbacks using Zephyr's native driver APIs.
    Prioritizes direct ISR callbacks over work queues for non-streamed
    inputs (CAN, UART, etc.) for lower latency.
    
    Supported peripherals:
    - CAN (interrupt-based ISR callbacks)
    - UART (interrupt-based ISR callbacks)
    - ADC (polled by engine for determinism)
    - SPI (polled by engine for determinism)
    - GPIO (polled by engine for determinism)
    
    Design: Only serial inputs (CAN/UART) use interrupts. ADC/GPIO/SPI
    are polled by lq_engine_step() for deterministic sampling.
    """
    
    def __init__(self):
        super().__init__("Zephyr RTOS")
    
    def generate_platform_header(self) -> str:
        """Zephyr driver headers."""
        return """/* Zephyr RTOS Platform Headers */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include "lq_platform.h"
#include "lq_hw_input.h"
#include "lq_generated.h"

/* Device handles from devicetree */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(can0), okay)
#define CAN0_DEV DEVICE_DT_GET(DT_NODELABEL(can0))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(can1), okay)
#define CAN1_DEV DEVICE_DT_GET(DT_NODELABEL(can1))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart0), okay)
#define UART0_DEV DEVICE_DT_GET(DT_NODELABEL(uart0))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)
#define UART1_DEV DEVICE_DT_GET(DT_NODELABEL(uart1))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(adc0), okay)
#define ADC0_DEV DEVICE_DT_GET(DT_NODELABEL(adc0))
#endif

/* ISR-safe buffers */
static uint8_t can_rx_data[8];
static uint8_t uart_rx_byte;
static uint16_t adc_sample_buffer[16];

"""
    
    def generate_isr_wrappers(self, nodes: List[Any]) -> str:
        """Generate Zephyr ISR callbacks (CAN/UART only)."""
        hw_inputs = [n for n in nodes if n.compatible.startswith('lq,hw-')]
        
        code = """/* ========================================
 * ISR Callbacks (CAN/UART only - for determinism)
 * ======================================== */

"""
        
        for node in hw_inputs:
            # Only generate ISRs for serial inputs
            if node.compatible in ['lq,hw-can-input', 'lq,hw-uart-input']:
                signal_id = node.signal_id or node.properties.get('signal_id', 0)
                isr = self._generate_node_isr(node, signal_id)
                if isr:
                    code += isr + "\n"
        
        # Generate polling functions for ADC/GPIO/SPI
        code += """/* ========================================
 * Polling Functions (ADC/GPIO/SPI - deterministic)
 * ======================================== */

"""
        
        for node in hw_inputs:
            if node.compatible in ['lq,hw-adc-input', 'lq,hw-gpio-input', 'lq,hw-spi-input']:
                signal_id = node.signal_id or node.properties.get('signal_id', 0)
                poll_fn = self._generate_polling_function(node, signal_id)
                if poll_fn:
                    code += poll_fn + "\n"
        
        return code
    
    def _generate_node_isr(self, node: Any, signal_id: int) -> str:
        """Generate ISR callback for serial inputs only (CAN/UART)."""
        if node.compatible == 'lq,hw-can-input':
            return self._generate_can_isr(node, signal_id)
        elif node.compatible == 'lq,hw-uart-input':
            return self._generate_uart_isr(node, signal_id)
        
        return ""
    
    def _generate_polling_function(self, node: Any, signal_id: int) -> str:
        """Generate polling function for deterministic inputs (ADC/GPIO/SPI)."""
        if node.compatible == 'lq,hw-adc-input':
            return self._generate_adc_polling(node, signal_id)
        elif node.compatible == 'lq,hw-spi-input':
            return self._generate_spi_polling(node, signal_id)
        elif node.compatible == 'lq,hw-gpio-input':
            return self._generate_gpio_polling(node, signal_id)
        
        return ""
    
    def _generate_can_isr(self, node: Any, signal_id: int) -> str:
        """Generate CAN ISR callback (no work queue)."""
        can_instance = node.properties.get('hw_instance', 0)
        can_id = node.properties.get('can_id', 0)
        is_extended = node.properties.get('extended_id', False)
        pgn = node.properties.get('pgn', None)
        
        filter_desc = f"PGN {pgn}" if pgn else f"ID 0x{can_id:X}"
        
        return f"""/* CAN RX Callback for {node.label} ({filter_desc}) */
static void lq_can_rx_callback_{node.label}(const struct device *dev,
                                             struct can_frame *frame,
                                             void *user_data)
{{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);
    
    /* Direct ISR callback - no work queue */
    uint32_t timestamp_us = k_ticks_to_us_floor32(k_uptime_ticks());
    
{self._generate_can_filter_check(node, pgn, can_id)}
    
    /* Push to layered queue engine */
    lq_hw_can_push({signal_id}, frame->data, frame->dlc, timestamp_us);
}}

/* CAN filter ID for {node.label} */
static int can_filter_id_{node.label} = -1;
"""
    
    def _generate_can_filter_check(self, node: Any, pgn: int, can_id: int) -> str:
        """Generate CAN ID/PGN filtering logic."""
        if pgn is not None:
            # J1939 PGN extraction from 29-bit extended ID
            return f"""    /* J1939 PGN filtering */
    uint32_t id = frame->id;
    uint32_t msg_pgn = (id >> 8) & 0x3FFFF;
    
    if (msg_pgn != {pgn}) {{
        return;  /* Not our PGN */
    }}
"""
        else:
            # Standard CAN ID check
            return f"""    /* Standard CAN ID check */
    if (frame->id != {can_id}) {{
        return;  /* Not our CAN ID */
    }}
"""
    
    def _generate_uart_isr(self, node: Any, signal_id: int) -> str:
        """Generate UART ISR callback (no work queue)."""
        uart_instance = node.properties.get('hw_instance', 0)
        
        return f"""/* UART RX Callback for {node.label} */
static void lq_uart_rx_callback_{node.label}(const struct device *dev,
                                              void *user_data)
{{
    ARG_UNUSED(user_data);
    
    uint8_t byte;
    
    while (uart_poll_in(dev, &byte) == 0) {{
        uint32_t timestamp_us = k_ticks_to_us_floor32(k_uptime_ticks());
        
        /* Push byte to layered queue engine */
        lq_hw_push({signal_id}, (uint32_t)byte, timestamp_us);
    }}
}}

/* UART RX interrupt setup for {node.label} */
static void lq_uart_setup_{node.label}(void)
{{
#ifdef UART{uart_instance}_DEV
    uart_irq_callback_user_data_set(UART{uart_instance}_DEV,
                                     lq_uart_rx_callback_{node.label},
                                     NULL);
    uart_irq_rx_enable(UART{uart_instance}_DEV);
#endif
}}
"""
    
    def _generate_adc_polling(self, node: Any, signal_id: int) -> str:
        """Generate ADC polling function (called from engine)."""
        adc_channel = node.properties.get('hw_channel', 0)
        adc_instance = node.properties.get('hw_instance', 0)
        
        return f"""/* ADC Polling for {node.label} (Channel {adc_channel}) */
static struct adc_sequence adc_seq_{node.label};
static int16_t adc_buf_{node.label};

/* Called by lq_engine_step() for deterministic sampling */
void lq_platform_poll_adc_{node.label}(void)
{{
#ifdef ADC{adc_instance}_DEV
    int ret = adc_read(ADC{adc_instance}_DEV, &adc_seq_{node.label});
    if (ret == 0) {{
        uint32_t timestamp_us = k_ticks_to_us_floor32(k_uptime_ticks());
        lq_hw_push({signal_id}, (uint32_t)adc_buf_{node.label}, timestamp_us);
    }}
#endif
}}

/* ADC channel configuration for {node.label} */
static void lq_adc_setup_{node.label}(void)
{{
#ifdef ADC{adc_instance}_DEV
    struct adc_channel_cfg channel_cfg = {{
        .channel_id = {adc_channel},
        .gain = ADC_GAIN_1,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    }};
    
    adc_channel_setup(ADC{adc_instance}_DEV, &channel_cfg);
    
    adc_seq_{node.label} = (struct adc_sequence){{
        .channels = BIT({adc_channel}),
        .buffer = &adc_buf_{node.label},
        .buffer_size = sizeof(adc_buf_{node.label}),
        .resolution = 12,  /* Adjust based on ADC capabilities */
    }};
#endif
}}
"""
    
    def _generate_spi_polling(self, node: Any, signal_id: int) -> str:
        """Generate SPI polling function (called from engine)."""
        spi_instance = node.properties.get('hw_instance', 0)
        cs_pin = node.properties.get('cs_pin', 0)
        num_bytes = node.properties.get('num_bytes', 2)
        
        return f"""/* SPI Polling for {node.label} */
static uint8_t spi_rx_buf_{node.label}[{num_bytes}];

/* Called by lq_engine_step() for deterministic sampling */
void lq_platform_poll_spi_{node.label}(void)
{{
    const struct spi_buf rx_buf = {{
        .buf = spi_rx_buf_{node.label},
        .len = {num_bytes}
    }};
    const struct spi_buf_set rx_bufs = {{
        .buffers = &rx_buf,
        .count = 1
    }};
    
    /* Deterministic SPI read from engine context */
    uint32_t timestamp_us = k_ticks_to_us_floor32(k_uptime_ticks());
    
    /* Convert bytes to value and push */
    uint32_t value = 0;
    for (int i = 0; i < {num_bytes}; i++) {{
        value = (value << 8) | spi_rx_buf_{node.label}[i];
    }}
    
    lq_hw_push({signal_id}, value, timestamp_us);
}}
"""
    
    def _generate_gpio_polling(self, node: Any, signal_id: int) -> str:
        """Generate GPIO polling function (called from engine)."""
        pin = node.properties.get('pin', 0)
        port = node.properties.get('port', 'gpio0')
        
        return f"""/* GPIO Polling for {node.label} (Pin {pin}) */

/* Called by lq_engine_step() for deterministic sampling */
void lq_platform_poll_gpio_{node.label}(void)
{{
    const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL({port}));
    uint32_t timestamp_us = k_ticks_to_us_floor32(k_uptime_ticks());
    int value = gpio_pin_get(gpio_dev, {pin});
    
    lq_hw_push({signal_id}, (uint32_t)value, timestamp_us);
}}
"""
    
    def generate_peripheral_init(self, nodes: List[Any]) -> str:
        """Generate Zephyr peripheral initialization (ISRs for CAN/UART only)."""
        hw_inputs = [n for n in nodes if n.compatible.startswith('lq,hw-')]
        
        can_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-can-input']
        uart_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-uart-input']
        adc_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-adc-input']
        gpio_nodes = [n for n in hw_inputs if n.compatible == 'lq,hw-gpio-input']
        
        code = """/* ========================================
 * Peripheral Initialization
 * ISRs: CAN/UART (async serial)
 * Polled: ADC/GPIO/SPI (deterministic)
 * ======================================== */

void lq_platform_hw_init(void)
{
"""
        
        # CAN initialization with callback filters
        if can_nodes:
            code += "    /* CAN: Setup ISR callbacks (no work queue) */\n"
            for node in can_nodes:
                can_instance = node.properties.get('hw_instance', 0)
                can_id = node.properties.get('can_id', 0)
                is_extended = node.properties.get('extended_id', False)
                pgn = node.properties.get('pgn', None)
                
                if pgn is not None:
                    # J1939 filter - accept PGN range
                    filter_id = (pgn << 8) & 0x1FFFFFFF
                    filter_mask = 0x03FFFF00  # Mask for PGN bits
                else:
                    filter_id = can_id
                    filter_mask = 0x1FFFFFFF if is_extended else 0x7FF
                
                code += f"""#ifdef CAN{can_instance}_DEV
    {{
        struct can_filter filter_{node.label} = {{
            .id = {filter_id},
            .mask = {filter_mask},
            .flags = CAN_FILTER_DATA | {'CAN_FILTER_IDE' if is_extended or pgn else '0'},
        }};
        
        can_filter_id_{node.label} = can_add_rx_filter(
            CAN{can_instance}_DEV,
            lq_can_rx_callback_{node.label},
            NULL,
            &filter_{node.label}
        );
        
        if (can_filter_id_{node.label} < 0) {{
            /* Handle error - filter installation failed */
        }}
    }}
#endif

"""
        
        # UART initialization
        if uart_nodes:
            code += "    /* UART: Setup interrupt-based RX */\n"
            for node in uart_nodes:
                code += f"    lq_uart_setup_{node.label}();\n"
            code += "\n"
        
        # ADC initialization
        if adc_nodes:
            code += "    /* ADC: Setup channels */\n"
            for node in adc_nodes:
                code += f"    lq_adc_setup_{node.label}();\n"
            code += "\n"
        
        # GPIO configuration (no interrupts - polled by engine)
        if gpio_nodes:
            code += "    /* GPIO: Configure as input (polled by engine) */\n"
            for node in gpio_nodes:
                pin = node.properties.get('pin', 0)
                port = node.properties.get('port', 'gpio0')
                
                code += f"""    {{
        const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL({port}));
        gpio_pin_configure(gpio_dev, {pin}, GPIO_INPUT);
    }}

"""
        
        code += "}\n"
        return code
    
    def generate_main(self, nodes: List[Any]) -> str:
        """Override to generate Zephyr-specific main."""
        content = self._header("Main entry point for Zephyr RTOS")
        content += """#include "lq_generated.h"
#include <zephyr/kernel.h>

void main(void)
{
    /* Initialize peripherals (ISRs for CAN/UART, config for ADC/GPIO/SPI) */
    lq_platform_hw_init();
    
    /* Initialize layered queue engine */
    lq_generated_init();
    
    printk("Layered Queue Driver started\\n");
    printk("ISRs: CAN/UART (async) | Polled: ADC/GPIO/SPI (deterministic)\\n");
    
    while (1) {
        uint64_t now_us = k_ticks_to_us_floor32(k_uptime_ticks());
        
        /* Process engine - polls ADC/GPIO/SPI, processes ISR-queued CAN/UART */
        lq_engine_step(&g_lq_engine, now_us);
        
        /* Dispatch outputs */
        lq_generated_dispatch_outputs();
        
        /* Sleep until next cycle */
        k_msleep(10);  /* 100Hz deterministic update rate */
    }
}
"""
        
        return content
