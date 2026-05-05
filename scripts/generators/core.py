"""Core code generator (lq_generated.h and lq_generated.c)."""

from typing import Dict, List, Set
from generators.base import Generator
from dts_parser import DTSNode


class CoreGenerator(Generator):
    """Generates lq_generated.h and lq_generated.c with engine struct and ISRs."""
    
    # Old Zephyr bindings that should raise exceptions (now handled by Zephyr platform drivers)
    DEPRECATED_ZEPHYR_BINDINGS = {
        'lq,hw-adc-input',
        'lq,hw-sensor-input', 
        'lq,hw-spi-input',
    }
    
    def generate(self, nodes: List[DTSNode], counts: Dict[str, int] = None) -> Dict[str, str]:
        """
        Generate core header and implementation.
        
        Args:
            nodes: List of DTSNode objects
            counts: Resource counts dict
            
        Returns:
            {'lq_generated.h': header_content, 'lq_generated.c': source_content}
        """
        # Check for deprecated Zephyr bindings
        self._check_deprecated_bindings(nodes)
        
        header = self._generate_header(nodes)
        source = self._generate_source(nodes, counts)
        
        return {
            'lq_generated.h': header,
            'lq_generated.c': source,
        }
    
    def _check_deprecated_bindings(self, nodes: List[DTSNode]):
        """Reject old Zephyr-specific bindings that are now handled by platform drivers."""
        deprecated_found = []
        for node in nodes:
            if node.compatible in self.DEPRECATED_ZEPHYR_BINDINGS:
                deprecated_found.append((node.label, node.compatible))
        
        if deprecated_found:
            msg = "Deprecated Zephyr bindings found. These are now handled by Zephyr platform drivers:\n"
            for label, compat in deprecated_found:
                msg += f"  - Node '{label}' uses '{compat}'\n"
            msg += "\nMigration guide:\n"
            msg += "  - Use generic 'lq,hw-input' with signal-type property instead\n"
            msg += "  - Platform-specific drivers (zephyr/drivers/) handle hardware details\n"
            msg += "  - See docs/dts-api-migration.md for examples\n"
            raise ValueError(msg)
