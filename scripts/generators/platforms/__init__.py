"""
Platform-specific code generators.

Each platform has its own generator class that implements platform-specific
ISR wrappers, peripheral initialization, and hardware configuration.
"""

from .baremetal import BaremetalGenerator
from .stm32 import STM32Generator
from .zephyr import ZephyrGenerator
from .esp32 import ESP32Generator
from .samd import SAMDGenerator
from .nrf52 import NRF52Generator
from .avr import AVRGenerator

# Platform registry
PLATFORM_GENERATORS = {
    'baremetal': BaremetalGenerator,
    'stm32': STM32Generator,
    'zephyr': ZephyrGenerator,
    'esp32': ESP32Generator,
    'samd': SAMDGenerator,
    'nrf52': NRF52Generator,
    'avr': AVRGenerator,
}


def get_platform_generator(platform: str):
    """
    Get platform-specific generator.
    
    Args:
        platform: Platform name (baremetal, stm32, esp32, etc.)
        
    Returns:
        Platform generator instance
        
    Raises:
        ValueError: If platform is unknown
    """
    if platform.lower() not in PLATFORM_GENERATORS:
        available = ', '.join(PLATFORM_GENERATORS.keys())
        raise ValueError(f"Unknown platform '{platform}'. Available: {available}")
    
    generator_class = PLATFORM_GENERATORS[platform.lower()]
    return generator_class()
