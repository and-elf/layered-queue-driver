"""Base class for platform-specific generators."""

from abc import ABC, abstractmethod
from typing import Dict, List, Any


class PlatformGenerator(ABC):
    """
    Base class for platform-specific code generation.
    
    Each platform generator creates:
    - ISR wrappers mapped to hardware interrupt vectors
    - Peripheral initialization (ADC, SPI, CAN, etc.)
    - Platform-specific headers and main entry point
    """
    
    def __init__(self, platform_name: str):
        """
        Initialize platform generator.
        
        Args:
            platform_name: Human-readable platform name
        """
        self.platform_name = platform_name
    
    def generate(self, nodes: List[Any], counts: Dict[str, int] = None) -> Dict[str, str]:
        """
        Generate all platform-specific files.
        
        Args:
            nodes: List of DTSNode objects
            counts: Resource counts (unused by most platforms)
            
        Returns:
            Dict mapping filename to content
        """
        outputs = {}
        
        # Generate hardware interface
        hw_code = self.generate_platform_hw(nodes)
        if hw_code:
            outputs['lq_platform_hw.c'] = hw_code
        
        # Generate main entry point
        main_code = self.generate_main(nodes)
        if main_code:
            outputs['main.c'] = main_code
        
        return outputs
    
    def generate_platform_hw(self, nodes: List[Any]) -> str:
        """
        Generate platform-specific hardware interface file.
        
        Args:
            nodes: List of DTSNode objects
            
        Returns:
            Complete lq_platform_hw.c content
        """
        content = self._header(f"Platform-specific hardware interface for {self.platform_name}")
        content += self.generate_platform_header()
        content += "\n"
        content += self.generate_isr_wrappers(nodes)
        content += "\n"
        content += self.generate_peripheral_init(nodes)
        
        return content
    
    @abstractmethod
    def generate_platform_header(self) -> str:
        """
        Generate platform-specific header includes.
        
        Returns:
            Include statements and typedefs
        """
        pass
    
    @abstractmethod
    def generate_isr_wrappers(self, nodes: List[Any]) -> str:
        """
        Generate ISR wrapper functions for hardware inputs.
        
        Args:
            nodes: List of DTSNode objects
            
        Returns:
            ISR wrapper code
        """
        pass
    
    @abstractmethod
    def generate_peripheral_init(self, nodes: List[Any]) -> str:
        """
        Generate peripheral initialization code.
        
        Args:
            nodes: List of DTSNode objects
            
        Returns:
            Peripheral init function
        """
        pass
    
    def generate_main(self, nodes: List[Any]) -> str:
        """
        Generate platform-specific main entry point.
        
        Args:
            nodes: List of DTSNode objects
            
        Returns:
            main.c content
        """
        content = self._header(f"Main entry point for {self.platform_name}")
        content += f"""#include "lq_generated.h"
#include <stdio.h>

int main(void) {{
    printf("Layered Queue Driver - {self.platform_name}\\n");
    
    /* Platform-specific initialization */
    lq_platform_peripherals_init();
    
    /* Initialize layered queue engine */
    if (lq_generated_init() != 0) {{
        printf("ERROR: Failed to initialize layered queue\\n");
        return 1;
    }}
    
    printf("Initialization complete\\n");
    
    /* Main event loop */
    while (1) {{
        /* Process output events */
        lq_generated_dispatch_outputs();
        
        /* Platform-specific tasks */
        {self._main_loop_body()}
    }}
    
    return 0;
}}
"""
        return content
    
    def _main_loop_body(self) -> str:
        """Platform-specific main loop body (can be overridden)."""
        return "/* Add platform-specific processing here */"
    
    def _header(self, description: str) -> str:
        """Generate standard file header."""
        return f"""/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * {description}
 */

"""
