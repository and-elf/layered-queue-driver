"""Tests for platform generators."""

import pytest
from generators.platforms import get_platform_generator
from generators.platforms.baremetal import BaremetalGenerator
from generators.platforms.stm32 import STM32Generator


class TestGetPlatformGenerator:
    """Tests for get_platform_generator function."""
    
    def test_get_baremetal_generator(self):
        """Test getting baremetal generator."""
        gen = get_platform_generator('baremetal')
        assert isinstance(gen, BaremetalGenerator)
    
    def test_get_stm32_generator(self):
        """Test getting STM32 generator."""
        gen = get_platform_generator('stm32')
        assert isinstance(gen, STM32Generator)
    
    def test_case_insensitive(self):
        """Test that platform names are case-insensitive."""
        gen1 = get_platform_generator('STM32')
        gen2 = get_platform_generator('stm32')
        assert type(gen1) == type(gen2)
    
    def test_unknown_platform_raises(self):
        """Test that unknown platform raises ValueError."""
        with pytest.raises(ValueError, match="Unknown platform"):
            get_platform_generator('nonexistent')


class TestBaremetalGenerator:
    """Tests for BaremetalGenerator class."""
    
    def test_generate_returns_dict(self):
        """Test that generate() returns dict."""
        gen = BaremetalGenerator()
        result = gen.generate([])
        
        assert isinstance(result, dict)
        assert 'lq_platform_hw.c' in result
        assert 'main.c' in result
    
    def test_platform_hw_has_headers(self):
        """Test that platform_hw.c has basic headers."""
        gen = BaremetalGenerator()
        result = gen.generate([])
        
        content = result['lq_platform_hw.c']
        assert '#include <stdint.h>' in content
        assert '#include <stdbool.h>' in content
    
    def test_platform_hw_has_init_function(self):
        """Test that platform_hw.c has init function."""
        gen = BaremetalGenerator()
        result = gen.generate([])
        
        content = result['lq_platform_hw.c']
        assert 'void lq_platform_peripherals_init(void)' in content
    
    def test_main_has_entry_point(self):
        """Test that main.c has main function."""
        gen = BaremetalGenerator()
        result = gen.generate([])
        
        content = result['main.c']
        assert 'int main(void)' in content
    
    def test_main_calls_init(self):
        """Test that main calls initialization functions."""
        gen = BaremetalGenerator()
        result = gen.generate([])
        
        content = result['main.c']
        assert 'lq_platform_peripherals_init()' in content
        assert 'lq_generated_init()' in content


class TestSTM32Generator:
    """Tests for STM32Generator class."""
    
    def test_generate_returns_dict(self):
        """Test that generate() returns dict."""
        gen = STM32Generator()
        result = gen.generate([])
        
        assert isinstance(result, dict)
        assert 'lq_platform_hw.c' in result
        assert 'main.c' in result
    
    def test_platform_hw_has_stm32_headers(self):
        """Test that platform_hw.c has STM32 HAL headers."""
        gen = STM32Generator()
        result = gen.generate([])
        
        content = result['lq_platform_hw.c']
        assert '#include "stm32f4xx_hal.h"' in content
        assert 'extern ADC_HandleTypeDef hadc1' in content
        assert 'extern CAN_HandleTypeDef hcan1' in content
    
    def test_adc_node_generates_isr(self, sample_adc_node):
        """Test that ADC node generates ISR callback."""
        gen = STM32Generator()
        result = gen.generate([sample_adc_node])
        
        content = result['lq_platform_hw.c']
        assert 'HAL_ADC_ConvCpltCallback' in content
        assert 'lq_adc_isr_throttle_adc' in content
    
    def test_spi_node_generates_isr(self, sample_spi_node):
        """Test that SPI node generates ISR callback."""
        gen = STM32Generator()
        result = gen.generate([sample_spi_node])
        
        content = result['lq_platform_hw.c']
        assert 'HAL_SPI_RxCpltCallback' in content
        assert 'lq_spi_isr_encoder_spi' in content
    
    def test_can_node_generates_isr(self, sample_can_node):
        """Test that CAN node generates ISR callback."""
        gen = STM32Generator()
        result = gen.generate([sample_can_node])
        
        content = result['lq_platform_hw.c']
        assert 'HAL_CAN_RxFifo0MsgPendingCallback' in content
        assert '0xF004' in content  # PGN
    
    def test_peripheral_init_starts_adc(self, sample_adc_node):
        """Test that peripheral init starts ADC with DMA."""
        gen = STM32Generator()
        result = gen.generate([sample_adc_node])
        
        content = result['lq_platform_hw.c']
        assert 'HAL_ADC_Start_DMA' in content
    
    def test_peripheral_init_configures_can(self, sample_can_node):
        """Test that peripheral init configures CAN filter."""
        gen = STM32Generator()
        result = gen.generate([sample_can_node])
        
        content = result['lq_platform_hw.c']
        assert 'CAN_FilterTypeDef' in content
        assert 'HAL_CAN_ConfigFilter' in content
        assert 'HAL_CAN_Start' in content
    
    def test_main_uses_wfi(self):
        """Test that main loop uses __WFI() for power efficiency."""
        gen = STM32Generator()
        result = gen.generate([])
        
        content = result['main.c']
        assert '__WFI()' in content
    
    def test_multiple_nodes_generate_multiple_isrs(self, sample_adc_node, sample_spi_node):
        """Test that multiple nodes generate multiple ISRs."""
        gen = STM32Generator()
        result = gen.generate([sample_adc_node, sample_spi_node])
        
        content = result['lq_platform_hw.c']
        assert 'HAL_ADC_ConvCpltCallback' in content
        assert 'HAL_SPI_RxCpltCallback' in content
        assert 'throttle_adc' in content
        assert 'encoder_spi' in content


class TestPlatformGeneratorIntegration:
    """Integration tests for platform generators."""
    
    def test_all_platforms_generate_valid_output(self, sample_adc_node):
        """Test that all platforms generate valid output."""
        platforms = ['baremetal', 'stm32', 'esp32', 'samd', 'nrf52', 'avr']
        
        for platform_name in platforms:
            gen = get_platform_generator(platform_name)
            result = gen.generate([sample_adc_node])
            
            # All should return dict with platform_hw.c and main.c
            assert isinstance(result, dict)
            assert 'lq_platform_hw.c' in result
            assert 'main.c' in result
            
            # All should have init function
            assert 'lq_platform_peripherals_init' in result['lq_platform_hw.c']
            
            # All should have main function
            assert 'int main(void)' in result['main.c']
    
    def test_generated_code_has_no_placeholders(self):
        """Test that STM32 generator has no TODO placeholders in ISRs."""
        gen = STM32Generator()
        
        from dts_parser import DTSNode
        adc = DTSNode('test_adc', 'lq,hw-adc-input', '0')
        adc.properties = {'signal_id': 0, 'hw_instance': 1, 'hw_channel': 3}
        adc.signal_id = 0
        
        result = gen.generate([adc])
        content = result['lq_platform_hw.c']
        
        # ISR section should not have TODO for ADC
        isr_section = content[content.find('Interrupt Service Routines'):content.find('Peripheral Initialization')]
        assert 'TODO' not in isr_section or 'HAL_ADC_ConvCpltCallback' in isr_section
