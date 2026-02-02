"""
Code generators for layered-queue-driver devicetree processing.

Each generator returns a dict[str, str] mapping filenames to their content.
The main orchestrator (dts_gen.py) collects all outputs and writes files.
"""

from .base import Generator
from .config import ConfigGenerator
from .core import CoreGenerator
from .uds import UDSGenerator
from .hil import HILGenerator
from .platform import PlatformGenerator

__all__ = [
    'Generator',
    'ConfigGenerator',
    'CoreGenerator',
    'UDSGenerator',
    'HILGenerator',
    'PlatformGenerator',
]
