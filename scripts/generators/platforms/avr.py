"""AVR platform generator."""

from typing import List, Any
from .base import PlatformGenerator


class AVRGenerator(PlatformGenerator):
    """AVR platform generator - stub implementation."""
    
    def __init__(self):
        super().__init__("AVR (Arduino)")
    
    def generate_platform_header(self) -> str:
        return """/* AVR Headers */
#include <avr/io.h>
#include <avr/interrupt.h>

/* TODO: Full AVR implementation */

"""
    
    def generate_isr_wrappers(self, nodes: List[Any]) -> str:
        return """/* TODO: AVR ISR wrappers */

"""
    
    def generate_peripheral_init(self, nodes: List[Any]) -> str:
        return """void lq_platform_peripherals_init(void) {
    /* TODO: AVR peripheral init */
}
"""
