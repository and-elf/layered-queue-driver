"""Baremetal/Native platform generator."""

from typing import List, Any
from .base import PlatformGenerator


class BaremetalGenerator(PlatformGenerator):
    """
    Baremetal/Native platform generator.
    
    Generates minimal stub implementation - user provides platform-specific code.
    Useful for native builds and custom embedded platforms.
    """
    
    def __init__(self):
        super().__init__("Baremetal/Native")
    
    def generate_platform_header(self) -> str:
        """Minimal header includes for baremetal."""
        return """/* Baremetal Platform - Minimal Headers */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

"""
    
    def generate_isr_wrappers(self, nodes: List[Any]) -> str:
        """No platform-specific ISRs for baremetal."""
        return """/* ========================================
 * Interrupt Service Routines
 * ======================================== */

/* Baremetal platform - ISRs are platform-specific
 * Implement hardware interrupt handlers in your platform code
 * that call the generic lq_*_isr_* functions from lq_generated.c
 */

"""
    
    def generate_peripheral_init(self, nodes: List[Any]) -> str:
        """Minimal peripheral init stub."""
        return """/* ========================================
 * Peripheral Initialization
 * ======================================== */

/* Baremetal/Native Platform Initialization
 * 
 * This is a stub - implement platform-specific initialization in your code.
 * For native builds, you typically don't need hardware init.
 * For baremetal targets, add your peripheral init here.
 */
void lq_platform_peripherals_init(void) {
    /* Add your platform-specific initialization here */
    
    /* Example for custom embedded platform:
     * - Configure clock tree
     * - Initialize ADC peripherals
     * - Configure SPI/CAN controllers
     * - Setup interrupt priorities
     * - Enable peripheral clocks
     */
}
"""
    
    def _main_loop_body(self) -> str:
        """Baremetal main loop - just a delay."""
        return """/* Sleep or yield to scheduler */
        /* For RTOS: osDelay(1); */
        /* For bare loop: __WFI(); */"""
