"""Platform-specific code generator (coordinator)."""

from typing import Dict, List
from .base import Generator
from .platforms import get_platform_generator


class PlatformGenerator(Generator):
    """
    Coordinates platform-specific code generation.
    
    This is a thin wrapper that delegates to platform-specific generators
    in the platforms/ subdirectory.
    """
    
    def __init__(self, platform: str = None):
        """
        Initialize platform generator.
        
        Args:
            platform: Target platform (stm32, samd, esp32, nrf52, baremetal, etc.)
        """
        self.platform = platform
        self._platform_gen = None
        
        if platform:
            try:
                self._platform_gen = get_platform_generator(platform)
            except ValueError as e:
                print(f"Warning: {e}")
                print(f"Falling back to baremetal stub")
                self._platform_gen = get_platform_generator('baremetal')
    
    def generate(self, nodes: List, counts: Dict[str, int] = None) -> Dict[str, str]:
        """
        Generate platform-specific code.
        
        Args:
            nodes: List of DTSNode objects
            counts: Resource counts dict (unused)
            
        Returns:
            {'lq_platform_hw.c': platform_code, 'main.c': main_code}
            or {} if no platform specified
        """
        if not self._platform_gen:
            return {}  # No platform specified
        
        return self._platform_gen.generate(nodes, counts)
