#!/usr/bin/env python3
"""
DTS to C Code Generator for Layered Queue Driver (Refactored)

This script parses devicetree files and orchestrates code generation:
- lq_config.h: Resource counts and configuration
- lq_generated.{h,c}: Engine struct and ISR handlers
- lq_generated_uds.{h,c}: UDS DID handlers (if UDS protocol used)
- lq_generated_test.dts: HIL test scenarios
- Platform-specific code (optional)

Usage:
    python3 scripts/dts_gen.py <input.dts> <output_dir> [--platform=stm32|esp32|...]

Examples:
    python3 scripts/dts_gen.py app.dts src/             # Generic (no platform ISRs)
    python3 scripts/dts_gen.py app.dts src/ --platform=stm32   # STM32 HAL ISRs
"""

import sys
import argparse
from pathlib import Path

# Add scripts directory to path for local imports
sys.path.insert(0, str(Path(__file__).parent))

from dts_parser import simple_dts_parser, resolve_phandles_and_assign_ids, calculate_resource_counts
from dts_validator import validate_all
from generators import ConfigGenerator, CoreGenerator, UDSGenerator, HILGenerator, PlatformGenerator


def main():
    """Main entry point - orchestrates all generators."""
    parser = argparse.ArgumentParser(description='Generate C code from devicetree')
    parser.add_argument('input_dts', help='Input devicetree file')
    parser.add_argument('output_dir', help='Output directory')
    parser.add_argument('--platform', help='Platform (stm32, samd, esp32, nrf52, zephyr, freertos, baremetal)')
    parser.add_argument('--expand-eds', action='store_true', help='Expand EDS references in DTS')
    parser.add_argument('--signals-header', help='Output path for signal ID header file')
    
    args = parser.parse_args()
    
    input_dts = Path(args.input_dts)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    if not input_dts.exists():
        print(f"Error: Input file {input_dts} not found")
        sys.exit(1)
    
    # Handle EDS expansion (legacy feature)
    if args.expand_eds:
        print("Warning: --expand-eds is deprecated, use Zephyr drivers instead")
        print("EDS expansion logic moved to separate tool")
        return
    
    # Parse DTS
    print(f"Parsing {input_dts}...")
    with open(input_dts, 'r') as f:
        dts_content = f.read()
    
    nodes = simple_dts_parser(dts_content)
    print(f"Found {len(nodes)} DTS nodes")
    
    # Resolve phandle references and auto-assign signal IDs
    nodes = resolve_phandles_and_assign_ids(nodes)
    
    # Validate configuration
    validate_all(nodes)
    
    # Calculate resource counts
    counts = calculate_resource_counts(nodes)
    print(f"Resources: {counts['num_signals']} signals, "
          f"{counts['num_hw_inputs']} hw_inputs, "
          f"{counts['num_merges']} merges, "
          f"{counts['num_cyclic_outputs']} outputs")
    
    # Orchestrate code generation
    # All generators return dict[filename, content] - no I/O performed
    outputs = {}
    
    # 1. Configuration
    config_gen = ConfigGenerator()
    outputs.update(config_gen.generate(nodes, counts))
    
    # 2. Core engine code
    core_gen = CoreGenerator()
    outputs.update(core_gen.generate(nodes, counts))
    
    # 3. UDS handlers (if applicable)
    uds_gen = UDSGenerator()
    outputs.update(uds_gen.generate(nodes, counts))
    
    # 4. HIL tests
    hil_gen = HILGenerator()
    outputs.update(hil_gen.generate(nodes, counts))
    
    # 5. Platform-specific code
    if args.platform:
        platform_gen = PlatformGenerator(args.platform)
        outputs.update(platform_gen.generate(nodes, counts))
    
    # Write all outputs atomically
    print(f"\nGenerating {len(outputs)} files...")
    for filename, content in outputs.items():
        output_path = output_dir / filename
        output_path.write_text(content)
        print(f"  ✓ {filename}")
    
    print(f"\nCode generation complete!")
    print(f"Output directory: {output_dir}")
    
    if not args.platform:
        print(f"\nTip: Add --platform=<name> to generate platform-specific ISRs")
        print(f"     Supported: stm32, samd, esp32, nrf52, baremetal")


if __name__ == '__main__':
    main()
