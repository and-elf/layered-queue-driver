"""Base class for all code generators."""

from abc import ABC, abstractmethod
from typing import Dict, List, Any


class Generator(ABC):
    """
    Base class for code generators.
    
    All generators must implement generate() which returns a dict
    mapping filenames to their content. No I/O is performed by generators.
    """
    
    @abstractmethod
    def generate(self, nodes: List[Any], counts: Dict[str, int] = None) -> Dict[str, str]:
        """
        Generate code from devicetree nodes.
        
        Args:
            nodes: List of DTSNode objects
            counts: Resource counts (signals, drivers, etc.)
            
        Returns:
            Dict mapping filename to file content
        """
        pass
    
    def _header(self, description: str) -> str:
        """Generate standard file header."""
        return f"""/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * {description}
 */

"""
