"""Tests for Zephyr platform generator."""

import pytest
from dts_parser import DTSNode
from generators.platforms.zephyr import ZephyrGenerator


class TestZephyrGenerator:
    """Test Zephyr RTOS platform generator."""
    
    def test_generate_returns_dict(self):
        """Test that generate returns a dict of files."""
        gen = ZephyrGenerator()
        result = gen.generate([])
        
        assert isinstance(result, dict)
        assert 'lq_platform_hw.c' in result
        assert 'main.c' in result
    
    def test_platform_hw_has_zephyr_headers(self):
        """Test that platform_hw includes Zephyr headers."""
        gen = ZephyrGenerator()
        result = gen.generate([])
        content = result['lq_platform_hw.c']
        
        assert '#include <zephyr/kernel.h>' in content
        assert '#include <zephyr/drivers/can.h>' in content
        assert '#include <zephyr/drivers/uart.h>' in content
        assert 'lq_platform.h' in content
    
    def test_can_node_generates_isr_callback(self):
        """Test CAN node generates direct ISR callback (no work queue)."""
        gen = ZephyrGenerator()
        
        can_node = DTSNode('engine_can', 'lq,hw-can-input', '0')
        can_node.properties = {
            'signal_id': 5,
            'hw_instance': 0,
            'can_id': 0x123,
            'extended_id': False,
        }
        can_node.signal_id = 5
        
        result = gen.generate([can_node])
        content = result['lq_platform_hw.c']
        
        # Should have callback function (not work queue)
        assert 'lq_can_rx_callback_engine_can' in content
        assert 'struct can_frame *frame' in content
        assert 'lq_hw_can_push' in content
        
        # Should NOT have work queue
        assert 'k_work' not in content or 'k_work_submit' not in content
        
        # Should have filter setup
        assert 'can_add_rx_filter' in content
        assert 'can_filter_id_engine_can' in content
    
    def test_can_j1939_pgn_filtering(self):
        """Test CAN with J1939 PGN generates correct filter."""
        gen = ZephyrGenerator()
        
        can_node = DTSNode('j1939_throttle', 'lq,hw-can-input', '0')
        can_node.properties = {
            'signal_id': 10,
            'hw_instance': 1,
            'pgn': 0xF004,
            'extended_id': True,
        }
        can_node.signal_id = 10
        
        result = gen.generate([can_node])
        content = result['lq_platform_hw.c']
        
        # Should extract PGN from extended ID
        assert 'msg_pgn = (id >> 8) & 0x3FFFF' in content
        assert 'if (msg_pgn != 0xF004)' in content or 'if (msg_pgn != 61444)' in content
        
        # Filter should use PGN-based mask
        assert 'can_add_rx_filter' in content
    
    def test_uart_node_generates_isr_callback(self):
        """Test UART node generates interrupt callback."""
        gen = ZephyrGenerator()
        
        uart_node = DTSNode('debug_uart', 'lq,hw-uart-input', '0')
        uart_node.properties = {
            'signal_id': 3,
            'hw_instance': 0,
        }
        uart_node.signal_id = 3
        
        result = gen.generate([uart_node])
        content = result['lq_platform_hw.c']
        
        # Should have UART RX callback
        assert 'lq_uart_rx_callback_debug_uart' in content
        assert 'uart_poll_in' in content
        assert 'uart_irq_callback_user_data_set' in content
        assert 'uart_irq_rx_enable' in content
        
        # Should push directly from ISR
        assert 'lq_hw_push' in content
    
    def test_adc_node_generates_sampling(self):
        """Test ADC node generates polling function (not ISR)."""
        gen = ZephyrGenerator()
        
        adc_node = DTSNode('throttle_adc', 'lq,hw-adc-input', '0')
        adc_node.properties = {
            'signal_id': 0,
            'hw_instance': 0,
            'hw_channel': 2,
        }
        adc_node.signal_id = 0
        
        result = gen.generate([adc_node])
        content = result['lq_platform_hw.c']
        
        # Should have ADC configuration
        assert 'adc_channel_cfg' in content
        assert 'adc_seq_throttle_adc' in content
        assert 'adc_channel_setup' in content
        
        # Should have POLLING function (not ISR)
        assert 'lq_platform_poll_adc_throttle_adc' in content
        assert 'Called by lq_engine_step()' in content
        
        # Should read ADC value
        assert 'adc_read' in content
        assert 'lq_hw_push' in content
    
    def test_gpio_interrupt(self):
        """Test GPIO node generates polling function (not interrupt)."""
        gen = ZephyrGenerator()
        
        gpio_node = DTSNode('door_sensor', 'lq,hw-gpio-input', '0')
        gpio_node.properties = {
            'signal_id': 15,
            'pin': 5,
            'port': 'gpio0',
        }
        gpio_node.signal_id = 15
        
        result = gen.generate([gpio_node])
        content = result['lq_platform_hw.c']
        
        # Should have GPIO POLLING function (not interrupt callback)
        assert 'lq_platform_poll_gpio_door_sensor' in content
        assert 'Called by lq_engine_step()' in content
        assert 'gpio_pin_get' in content
        
        # Should configure as input (no interrupt setup)
        assert 'gpio_pin_configure' in content
        assert 'GPIO_INPUT' in content
        
        # Should NOT have interrupt callbacks
        assert 'gpio_callback' not in content
        assert 'gpio_init_callback' not in content
        assert 'gpio_pin_interrupt_configure' not in content
    
    def test_peripheral_init_no_work_queues(self):
        """Test peripheral init uses ISRs only for CAN/UART."""
        gen = ZephyrGenerator()
        
        can_node = DTSNode('can0', 'lq,hw-can-input', '0')
        can_node.properties = {'signal_id': 1, 'hw_instance': 0, 'can_id': 0x100}
        can_node.signal_id = 1
        
        result = gen.generate([can_node])
        content = result['lq_platform_hw.c']
        
        # Should have init function
        assert 'void lq_platform_hw_init(void)' in content
        
        # Should use direct callbacks for CAN
        assert 'can_add_rx_filter' in content
        
        # Should NOT initialize work queues or message queues
        init_section = content[content.find('lq_platform_hw_init'):content.find('lq_platform_hw_init') + 2000]
        assert 'k_work_init' not in init_section
        assert 'k_msgq_init' not in init_section
        
        # Should mention deterministic strategy
        assert 'Polled: ADC/GPIO/SPI (deterministic)' in content or 'deterministic' in content
    
    def test_main_loop_periodic_adc(self):
        """Test main loop is simple - engine handles polling."""
        gen = ZephyrGenerator()
        
        adc_node = DTSNode('voltage_adc', 'lq,hw-adc-input', '0')
        adc_node.properties = {'signal_id': 2, 'hw_instance': 0, 'hw_channel': 1}
        adc_node.signal_id = 2
        
        result = gen.generate([adc_node])
        content = result['main.c']
        
        # Should have main loop
        assert 'void main(void)' in content
        assert 'while (1)' in content
        
        # Should call engine processing (which polls ADC)
        assert 'lq_engine_step' in content
        assert 'k_msleep' in content
        
        # Should NOT call ADC sampling directly (engine does it)
        assert 'lq_adc_sample_voltage_adc' not in content
        assert 'lq_platform_poll_adc_voltage_adc' not in content  # Only in platform_hw.c
    
    def test_multiple_can_filters(self):
        """Test multiple CAN nodes generate separate filters."""
        gen = ZephyrGenerator()
        
        can1 = DTSNode('can_rpm', 'lq,hw-can-input', '0')
        can1.properties = {'signal_id': 1, 'hw_instance': 0, 'can_id': 0x100}
        can1.signal_id = 1
        
        can2 = DTSNode('can_temp', 'lq,hw-can-input', '0')
        can2.properties = {'signal_id': 2, 'hw_instance': 0, 'can_id': 0x200}
        can2.signal_id = 2
        
        result = gen.generate([can1, can2])
        content = result['lq_platform_hw.c']
        
        # Should have separate callbacks
        assert 'lq_can_rx_callback_can_rpm' in content
        assert 'lq_can_rx_callback_can_temp' in content
        
        # Should have separate filter IDs
        assert 'can_filter_id_can_rpm' in content
        assert 'can_filter_id_can_temp' in content
        
        # Should install both filters
        assert content.count('can_add_rx_filter') >= 2


class TestZephyrGeneratorIntegration:
    """Integration tests for Zephyr generator."""
    
    def test_mixed_peripherals(self):
        """Test generating code with CAN (ISR), UART (ISR), ADC/GPIO (polled)."""
        gen = ZephyrGenerator()
        
        can_node = DTSNode('can_in', 'lq,hw-can-input', '0')
        can_node.properties = {'signal_id': 0, 'hw_instance': 0, 'can_id': 0x123}
        can_node.signal_id = 0
        
        uart_node = DTSNode('uart_in', 'lq,hw-uart-input', '0')
        uart_node.properties = {'signal_id': 1, 'hw_instance': 0}
        uart_node.signal_id = 1
        
        adc_node = DTSNode('adc_in', 'lq,hw-adc-input', '0')
        adc_node.properties = {'signal_id': 2, 'hw_instance': 0, 'hw_channel': 0}
        adc_node.signal_id = 2
        
        gpio_node = DTSNode('gpio_in', 'lq,hw-gpio-input', '0')
        gpio_node.properties = {'signal_id': 3, 'pin': 10, 'port': 'gpio0'}
        gpio_node.signal_id = 3
        
        nodes = [can_node, uart_node, adc_node, gpio_node]
        result = gen.generate(nodes)
        
        hw_content = result['lq_platform_hw.c']
        
        # Should have ISR callbacks for serial inputs
        assert 'lq_can_rx_callback_can_in' in hw_content
        assert 'lq_uart_rx_callback_uart_in' in hw_content
        
        # Should have POLLING functions for deterministic inputs
        assert 'lq_platform_poll_adc_adc_in' in hw_content
        assert 'lq_platform_poll_gpio_gpio_in' in hw_content
        
        # Should initialize appropriately
        assert 'can_add_rx_filter' in hw_content
        assert 'uart_irq_rx_enable' in hw_content
        assert 'adc_channel_setup' in hw_content
        assert 'gpio_pin_configure' in hw_content
        
        # Should NOT have GPIO interrupts
        assert 'gpio_pin_interrupt_configure' not in hw_content
    
    def test_no_work_queues_in_generated_code(self):
        """Verify generated code doesn't use work queues for CAN/UART."""
        gen = ZephyrGenerator()
        
        can_node = DTSNode('test_can', 'lq,hw-can-input', '0')
        can_node.properties = {'signal_id': 0, 'hw_instance': 0, 'can_id': 0x456}
        can_node.signal_id = 0
        
        result = gen.generate([can_node])
        hw_content = result['lq_platform_hw.c']
        
        # Verify ISR-based approach
        assert 'can_add_rx_filter' in hw_content  # Direct callback registration
        assert 'lq_can_rx_callback_test_can' in hw_content
        
        # Verify NO work queue usage
        # (k_work might appear in comments, but not in actual init/callback code)
        isr_section = hw_content[hw_content.find('lq_can_rx_callback'):hw_content.find('lq_can_rx_callback') + 1000]
        assert 'k_work_submit' not in isr_section
        assert 'k_msgq' not in isr_section
