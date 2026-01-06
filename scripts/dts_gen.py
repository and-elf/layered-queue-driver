#!/usr/bin/env python3
"""
DTS to C Code Generator for Layered Queue Driver

This script parses devicetree files and generates:
- lq_generated.h: Forward declarations and extern definitions
- lq_generated.c: Complete engine struct initialization and ISR handlers
- lq_platform_hw.c: Platform-specific ISRs and peripheral init (optional)

Usage:
    python3 scripts/dts_gen.py <input.dts> <output_dir> [--platform=stm32|samd|esp32|nrf52|baremetal]

Examples:
    python3 scripts/dts_gen.py app.dts src/             # Generic (no platform ISRs)
    python3 scripts/dts_gen.py app.dts src/ --platform=stm32   # STM32 HAL ISRs
    python3 scripts/dts_gen.py app.dts src/ --platform=esp32   # ESP32 IDF ISRs
"""

import sys
import re
from pathlib import Path

# Import platform adaptors if available
try:
    from platform_adaptors import get_platform_adaptor
    PLATFORM_SUPPORT = True
except ImportError:
    PLATFORM_SUPPORT = False
    print("Warning: platform_adaptors.py not found. Platform-specific generation disabled.")

class DTSNode:
    def __init__(self, label, compatible, address=None):
        self.label = label
        self.compatible = compatible
        self.address = address
        self.properties = {}
        self.children = []

def parse_property_value(value):
    """Parse DTS property value - handle <>, "", arrays"""
    value = value.strip().rstrip(';')
    
    # Phandle reference: <&sensor>
    if value.startswith('<&'):
        return value[2:-1]
    
    # Array of integers: <1 2 3>
    if value.startswith('<') and value.endswith('>'):
        inner = value[1:-1].strip()
        # Check if it contains phandle
        if '&' in inner:
            return inner
        nums = inner.split()
        if len(nums) == 1:
            try:
                return int(nums[0], 0)  # Single value
            except ValueError:
                return nums[0]
        try:
            return [int(n, 0) for n in nums]  # Array
        except ValueError:
            return nums
    
    # String: "median"
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    
    # Boolean flag (property exists with no value)
    if not value:
        return True
    
    return value

def simple_dts_parser(dts_content):
    """Simplified DTS parser - extracts compatible nodes with properties"""
    nodes = []
    
    # Remove comments
    dts_content = re.sub(r'//.*?\n', '\n', dts_content)
    dts_content = re.sub(r'/\*.*?\*/', '', dts_content, flags=re.DOTALL)
    
    # Find all node definitions
    # Pattern: label: node-name@addr { ... }
    node_pattern = r'(\w+):\s*[\w-]+(?:@([\w]+))?\s*\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}'
    
    for match in re.finditer(node_pattern, dts_content):
        label = match.group(1)
        address = match.group(2)
        content = match.group(3)
        
        # Extract compatible
        compat_match = re.search(r'compatible\s*=\s*"([^"]+)"', content)
        if not compat_match:
            continue
        compatible = compat_match.group(1)
        
        node = DTSNode(label, compatible, address)
        
        # Extract properties
        prop_pattern = r'([\w-]+)\s*=\s*([^;]+);'
        for prop_match in re.finditer(prop_pattern, content):
            prop_name = prop_match.group(1).replace('-', '_')
            prop_value = parse_property_value(prop_match.group(2))
            node.properties[prop_name] = prop_value
        
        # Check for boolean properties (no value) - standalone keywords
        bool_props = ['signed', 'check_staleness', 'check_range', 'check_status']
        for bool_prop in bool_props:
            # Convert from kebab-case to snake_case for matching
            dts_prop = bool_prop.replace('_', '-')
            if re.search(rf'\b{dts_prop}\b', content) and bool_prop not in node.properties:
                node.properties[bool_prop] = True
        
        nodes.append(node)
    
    return nodes

def generate_header(nodes, output_path):
    """Generate lq_generated.h with declarations"""
    
    # Collect hardware inputs for ISR declarations
    hw_inputs = [n for n in nodes if n.compatible.startswith('lq,hw-')]
    
    # Collect fault monitors for wake function declarations
    fault_monitors = [n for n in nodes if n.compatible == 'lq,fault-monitor']
    
    with open(output_path, 'w') as f:
        f.write("""/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 */

#ifndef LQ_GENERATED_H_
#define LQ_GENERATED_H_

#include "lq_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Engine instance */
extern struct lq_engine g_lq_engine;

/* Initialization function */
int lq_generated_init(void);

""")
        
        # Add ISR handler declarations
        if hw_inputs:
            f.write("/* Hardware ISR handlers */\n")
            for hw in hw_inputs:
                if 'adc' in hw.compatible:
                    f.write(f"void lq_adc_isr_{hw.label}(uint16_t value);\n")
                elif 'spi' in hw.compatible:
                    f.write(f"void lq_spi_isr_{hw.label}(int32_t value);\n")
            f.write("\n")
        
        # Add fault wake function declarations
        if fault_monitors:
            wake_functions = set()
            for fm in fault_monitors:
                wake_fn = fm.properties.get('wake_function')
                if wake_fn:
                    wake_functions.add(wake_fn)
            
            if wake_functions:
                f.write("/* Fault monitor wake callbacks */\n")
                for wake_fn in sorted(wake_functions):
                    f.write(f"void {wake_fn}(uint8_t monitor_id, int32_t input_value, enum lq_fault_level fault_level);\n")
                f.write("\n")
        
        f.write("""#ifdef __cplusplus
}
#endif

#endif /* LQ_GENERATED_H_ */
""")

def generate_source(nodes, output_path):
    """Generate lq_generated.c with engine struct and ISRs"""
    
    # Categorize nodes
    engine_node = None
    hw_inputs = []
    merges = []
    fault_monitors = []
    cyclic_outputs = []
    
    for node in nodes:
        if node.compatible == 'lq,engine':
            engine_node = node
        elif node.compatible.startswith('lq,hw-'):
            hw_inputs.append(node)
        elif node.compatible == 'lq,mid-merge':
            merges.append(node)
        elif node.compatible == 'lq,fault-monitor':
            fault_monitors.append(node)
        elif node.compatible == 'lq,cyclic-output':
            cyclic_outputs.append(node)
    
    # Calculate maximum signal ID
    max_signal_id = 0
    for node in hw_inputs:
        signal_id = node.properties.get('signal_id', 0)
        max_signal_id = max(max_signal_id, signal_id)
    for node in merges:
        output_id = node.properties.get('output_signal_id', 0)
        input_ids = node.properties.get('input_signal_ids', [])
        if isinstance(input_ids, int):
            input_ids = [input_ids]
        max_signal_id = max(max_signal_id, output_id)
        if input_ids:
            max_signal_id = max(max_signal_id, max(input_ids))
    for node in cyclic_outputs:
        source_id = node.properties.get('source_signal_id', 0)
        max_signal_id = max(max_signal_id, source_id)
    for node in fault_monitors:
        input_id = node.properties.get('input_signal_id', 0)
        output_id = node.properties.get('fault_output_signal_id', 0)
        max_signal_id = max(max_signal_id, input_id, output_id)
    
    num_signals = max_signal_id + 1  # +1 because IDs are 0-indexed
    
    with open(output_path, 'w') as f:
        f.write("""/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 */

#include "lq_generated.h"
#include "lq_hw_input.h"
#include "lq_common.h"
#include "lq_event.h"
#include "lq_hil.h"
#include <string.h>

""")
        
        # Generate engine instance with inline array initialization
        f.write("/* Engine instance */\n")
        f.write("struct lq_engine g_lq_engine = {\n")
        f.write(f"    .num_signals = {num_signals},\n")
        f.write(f"    .num_merges = {len(merges)},\n")
        f.write(f"    .num_fault_monitors = {len(fault_monitors)},\n")
        f.write(f"    .num_cyclic_outputs = {len(cyclic_outputs)},\n")
        
        # Inline merge contexts
        if merges:
            f.write("    .merges = {\n")
            for i, node in enumerate(merges):
                vote_method_map = {
                    'median': 'LQ_VOTE_MEDIAN',
                    'average': 'LQ_VOTE_AVERAGE',
                    'min': 'LQ_VOTE_MIN',
                    'max': 'LQ_VOTE_MAX',
                }
                vote_method = vote_method_map.get(node.properties.get('voting_method', 'median'))
                
                input_ids = node.properties.get('input_signal_ids', [])
                if isinstance(input_ids, int):
                    input_ids = [input_ids]
                
                f.write(f"        [{i}] = {{\n")
                f.write(f"            .output_signal = {node.properties.get('output_signal_id', 0)},\n")
                f.write(f"            .input_signals = {{{', '.join(map(str, input_ids))}}},\n")
                f.write(f"            .num_inputs = {len(input_ids)},\n")
                f.write(f"            .voting_method = {vote_method},\n")
                f.write(f"            .tolerance = {node.properties.get('tolerance', 0)},\n")
                f.write(f"            .stale_us = {node.properties.get('stale_us', 0)},\n")
                f.write(f"            .enabled = true,\n")
                f.write(f"        }},\n")
            f.write("    },\n")
        
        # Inline fault monitor contexts
        if fault_monitors:
            f.write("    .fault_monitors = {\n")
            for i, node in enumerate(fault_monitors):
                f.write(f"        [{i}] = {{\n")
                f.write(f"            .input_signal = {node.properties.get('input_signal_id', 0)},\n")
                f.write(f"            .fault_output_signal = {node.properties.get('fault_output_signal_id', 0)},\n")
                
                # Fault condition flags
                check_staleness = 'check_staleness' in node.properties
                check_range = 'check_range' in node.properties
                check_status = 'check_status' in node.properties
                
                f.write(f"            .check_staleness = {'true' if check_staleness else 'false'},\n")
                if check_staleness:
                    f.write(f"            .stale_timeout_us = {node.properties.get('stale_timeout_us', 1000000)},\n")
                else:
                    f.write(f"            .stale_timeout_us = 0,\n")
                
                f.write(f"            .check_range = {'true' if check_range else 'false'},\n")
                if check_range:
                    f.write(f"            .min_value = {node.properties.get('min_value', 0)},\n")
                    f.write(f"            .max_value = {node.properties.get('max_value', 65535)},\n")
                else:
                    f.write(f"            .min_value = 0,\n")
                    f.write(f"            .max_value = 0,\n")
                
                f.write(f"            .check_status = {'true' if check_status else 'false'},\n")
                
                # Fault level
                fault_level = node.properties.get('fault_level', 1)
                f.write(f"            .fault_level = {fault_level},\n")
                
                # Wake function
                wake_fn = node.properties.get('wake_function')
                if wake_fn:
                    f.write(f"            .wake = {wake_fn},\n")
                else:
                    f.write(f"            .wake = NULL,\n")
                
                f.write(f"            .enabled = true,\n")
                f.write(f"        }},\n")
            f.write("    },\n")
        
        # Inline cyclic output contexts
        if cyclic_outputs:
            f.write("    .cyclic_outputs = {\n")
            for i, node in enumerate(cyclic_outputs):
                output_type_map = {
                    'can': 'LQ_OUTPUT_CAN',
                    'j1939': 'LQ_OUTPUT_J1939',
                    'canopen': 'LQ_OUTPUT_CANOPEN',
                    'gpio': 'LQ_OUTPUT_GPIO',
                    'uart': 'LQ_OUTPUT_UART',
                }
                output_type = output_type_map.get(node.properties.get('output_type', 'can'))
                
                f.write(f"        [{i}] = {{\n")
                f.write(f"            .type = {output_type},\n")
                f.write(f"            .target_id = {node.properties.get('target_id', 0)},\n")
                f.write(f"            .source_signal = {node.properties.get('source_signal_id', 0)},\n")
                f.write(f"            .period_us = {node.properties.get('period_us', 100000)},\n")
                f.write(f"            .next_deadline = {node.properties.get('deadline_offset_us', 0)},\n")
                f.write(f"            .flags = 0,\n")
                f.write(f"            .enabled = true,\n")
                f.write(f"        }},\n")
            f.write("    },\n")
        
        f.write("};\n\n")
        
        # Generate ISR handlers for hardware inputs
        for node in hw_inputs:
            signal_id = node.properties.get('signal_id', 0)
            
            if node.compatible == 'lq,hw-adc-input':
                f.write(f"/* ADC ISR for {node.label} */\n")
                f.write(f"void lq_adc_isr_{node.label}(uint16_t value) {{\n")
                f.write(f"    lq_hw_push({signal_id}, (uint32_t)value);\n")
                f.write(f"}}\n\n")
            
            elif node.compatible == 'lq,hw-spi-input':
                f.write(f"/* SPI ISR for {node.label} */\n")
                f.write(f"void lq_spi_isr_{node.label}(int32_t value) {{\n")
                f.write(f"    lq_hw_push({signal_id}, (uint32_t)value);\n")
                f.write(f"}}\n\n")
        
        # Generate weak stub implementations for fault wake functions
        wake_functions = set()
        for fm in fault_monitors:
            wake_fn = fm.properties.get('wake_function')
            if wake_fn:
                wake_functions.add(wake_fn)
        
        if wake_functions:
            f.write("/* Fault monitor wake callbacks - weak stubs (user can override) */\n")
            for wake_fn in sorted(wake_functions):
                f.write(f"__weak\n")
                f.write(f"void {wake_fn}(uint8_t monitor_id, int32_t input_value, enum lq_fault_level fault_level) {{\n")
                f.write(f"    /* Default: no action. Override this function to implement safety response. */\n")
                f.write(f"    (void)monitor_id;\n")
                f.write(f"    (void)input_value;\n")
                f.write(f"    (void)fault_level;\n")
                f.write(f"}}\n\n")
        
        # Generate init function
        f.write("/* Initialization */\n")
        f.write("int lq_generated_init(void) {\n")
        f.write("    /* Auto-detect HIL mode on native platform (if not already initialized) */\n")
        f.write("    #ifdef LQ_PLATFORM_NATIVE\n")
        f.write("    if (!lq_hil_is_active()) {\n")
        f.write("        lq_hil_init(LQ_HIL_MODE_DISABLED, 0);  /* Auto-detects from env */\n")
        f.write("    }\n")
        f.write("    #endif\n")
        f.write("    \n")
        f.write("    /* Initialize engine */\n")
        f.write("    int ret = lq_engine_init(&g_lq_engine);\n")
        f.write("    if (ret != 0) return ret;\n")
        f.write("    \n")
        f.write("    /* Hardware input layer */\n")
        f.write("    ret = lq_hw_input_init(64);\n")
        f.write("    if (ret != 0) return ret;\n")
        f.write("    \n")
        f.write("    /* Platform-specific peripheral init */\n")
        f.write("    #ifdef LQ_PLATFORM_INIT\n")
        f.write("    lq_platform_peripherals_init();\n")
        f.write("    #endif\n")
        f.write("    \n")
        f.write("    return 0;\n")
        f.write("}\n")

def generate_hil_tests(nodes, output_path):
    """Auto-generate HIL tests from system definition"""
    
    # Collect all inputs
    adc_sources = [n for n in nodes if 'adc' in n.compatible]
    spi_sources = [n for n in nodes if 'spi' in n.compatible]
    can_sources = [n for n in nodes if 'can' in n.compatible]
    merge_nodes = [n for n in nodes if 'merge' in n.compatible or 'voter' in n.compatible]
    error_nodes = [n for n in nodes if 'error' in n.compatible]
    output_nodes = [n for n in nodes if 'cyclic-output' in n.compatible or 'can-output' in n.compatible]
    
    with open(output_path, 'w') as f:
        f.write("/*\n")
        f.write(" * AUTO-GENERATED HIL Tests\n")
        f.write(" * Generated from system DTS\n")
        f.write(" * DO NOT EDIT MANUALLY\n")
        f.write(" */\n\n")
        f.write("/ {\n")
        
        # Test 1: All inputs nominal
        f.write("    hil-test-all-inputs-nominal {\n")
        f.write("        compatible = \"lq,hil-test\";\n")
        f.write("        description = \"All inputs at nominal values\";\n")
        f.write("        timeout-ms = <2000>;\n")
        f.write("        \n")
        f.write("        sequence {\n")
        
        step = 0
        # Inject all ADC inputs
        for adc in adc_sources:
            channel = adc.properties.get('channel', 0)
            value = adc.properties.get('nominal-value', 2500)
            f.write(f"            step@{step} {{\n")
            f.write(f"                action = \"inject-adc\";\n")
            f.write(f"                channel = <{channel}>;\n")
            f.write(f"                value = <{value}>;\n")
            f.write(f"            }};\n")
            step += 1
        
        # Inject all CAN inputs
        for can in can_sources:
            pgn = can.properties.get('pgn', 61444)
            f.write(f"            step@{step} {{\n")
            f.write(f"                action = \"inject-can-pgn\";\n")
            f.write(f"                pgn = <{pgn}>;\n")
            f.write(f"                priority = <3>;\n")
            f.write(f"                source-addr = <0x00>;\n")
            f.write(f"                data = [0xE8 0x5E 0x00 0x00 0x00 0x00 0x00 0x00];\n")
            f.write(f"            }};\n")
            step += 1
        
        # Expect output
        if output_nodes:
            output = output_nodes[0]
            pgn = output.properties.get('pgn', 61444)
            f.write(f"            step@{step} {{\n")
            f.write(f"                action = \"expect-can\";\n")
            f.write(f"                pgn = <{pgn}>;\n")
            f.write(f"                timeout-ms = <200>;\n")
            f.write(f"            }};\n")
        
        f.write("        };\n")
        f.write("    };\n\n")
        
        # Test 2: Voting/merge behavior
        if merge_nodes:
            merge = merge_nodes[0]
            f.write("    hil-test-voting-merge {\n")
            f.write("        compatible = \"lq,hil-test\";\n")
            f.write("        description = \"Test voting/merge logic\";\n")
            f.write("        timeout-ms = <2000>;\n")
            f.write("        \n")
            f.write("        sequence {\n")
            
            step = 0
            # Inject slightly different values
            for i, adc in enumerate(adc_sources[:3]):  # First 3 sensors
                channel = adc.properties.get('channel', i)
                value = 3000 + (i * 5)  # 3000, 3005, 3010
                f.write(f"            step@{step} {{\n")
                f.write(f"                action = \"inject-adc\";\n")
                f.write(f"                channel = <{channel}>;\n")
                f.write(f"                value = <{value}>;\n")
                f.write(f"            }};\n")
                step += 1
            
            # Verify merged output
            if output_nodes:
                output = output_nodes[0]
                pgn = output.properties.get('pgn', 61444)
                f.write(f"            step@{step} {{\n")
                f.write(f"                action = \"expect-can\";\n")
                f.write(f"                pgn = <{pgn}>;\n")
                f.write(f"                timeout-ms = <200>;\n")
                f.write(f"            }};\n")
            
            f.write("        };\n")
            f.write("    };\n\n")
        
        # Test 3: Error detection
        if error_nodes and adc_sources:
            error = error_nodes[0]
            adc = adc_sources[0]
            channel = adc.properties.get('channel', 0)
            
            f.write("    hil-test-error-detection {\n")
            f.write("        compatible = \"lq,hil-test\";\n")
            f.write("        description = \"Test error detection and DM1\";\n")
            f.write("        timeout-ms = <3000>;\n")
            f.write("        \n")
            f.write("        sequence {\n")
            f.write("            step@0 {\n")
            f.write("                action = \"inject-adc\";\n")
            f.write(f"                channel = <{channel}>;\n")
            f.write("                value = <9999>;  /* Out of range */\n")
            f.write("            };\n")
            f.write("            step@1 {\n")
            f.write("                action = \"expect-can\";\n")
            f.write("                pgn = <65226>;  /* DM1 */\n")
            f.write("                timeout-ms = <1500>;\n")
            f.write("            };\n")
            f.write("        };\n")
            f.write("    };\n\n")
        
        # Test 4: Latency test
        if adc_sources and output_nodes:
            f.write("    hil-test-latency {\n")
            f.write("        compatible = \"lq,hil-test\";\n")
            f.write("        description = \"End-to-end latency\";\n")
            f.write("        timeout-ms = <1000>;\n")
            f.write("        \n")
            f.write("        sequence {\n")
            f.write("            step@0 {\n")
            f.write("                action = \"measure-latency\";\n")
            f.write("                max-latency-us = <50000>;  /* 50ms */\n")
            f.write("            };\n")
            f.write("        };\n")
            f.write("    };\n\n")
        
        f.write("};\n")

def generate_platform_hw(nodes, output_path, platform):
    """Generate platform-specific hardware interface"""
    if not PLATFORM_SUPPORT:
        print(f"Skipping platform-specific generation (platform_adaptors.py not found)")
        return
    
    try:
        adaptor = get_platform_adaptor(platform)
    except ValueError as e:
        print(f"Error: {e}")
        return
    
    # Collect hardware input nodes
    hw_inputs = [n for n in nodes if n.compatible.startswith('lq,hw-')]
    
    with open(output_path, 'w') as f:
        f.write(f"""/*
 * AUTO-GENERATED PLATFORM-SPECIFIC CODE
 * Platform: {adaptor.platform_name}
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * This file contains real hardware ISRs and peripheral configuration
 * for flashing directly to {adaptor.platform_name} hardware.
 */

""")
        
        # Platform headers
        f.write(adaptor.generate_platform_header())
        f.write("\n")
        
        # Generate ISR wrappers for each hardware input
        f.write("/* ========================================\n")
        f.write(" * Interrupt Service Routines\n")
        f.write(" * ======================================== */\n\n")
        
        for node in hw_inputs:
            signal_id = node.properties.get('signal_id', 0)
            isr_code = adaptor.generate_isr_wrapper(node, signal_id)
            if isr_code:
                f.write(isr_code)
                f.write("\n")
        
        # Generate peripheral initialization
        f.write("/* ========================================\n")
        f.write(" * Peripheral Initialization\n")
        f.write(" * ======================================== */\n\n")
        
        f.write(adaptor.generate_peripheral_init(hw_inputs))
    
    print(f"Generated {output_path} for {adaptor.platform_name}")

def main():
    # Parse command line arguments
    platform = None
    args = sys.argv[1:]
    
    # Extract --platform= argument
    filtered_args = []
    for arg in args:
        if arg.startswith('--platform='):
            platform = arg.split('=')[1]
        else:
            filtered_args.append(arg)
    
    if len(filtered_args) != 2:
        print(f"Usage: {sys.argv[0]} <input.dts> <output_dir> [--platform=stm32|samd|esp32|nrf52|baremetal]")
        print(f"\nExamples:")
        print(f"  {sys.argv[0]} app.dts src/")
        print(f"  {sys.argv[0]} app.dts src/ --platform=stm32")
        print(f"  {sys.argv[0]} app.dts src/ --platform=esp32")
        sys.exit(1)
    
    input_dts = Path(filtered_args[0])
    output_dir = Path(filtered_args[1])
    output_dir.mkdir(parents=True, exist_ok=True)
    
    if not input_dts.exists():
        print(f"Error: Input file {input_dts} not found")
        sys.exit(1)
    
    # Parse DTS
    with open(input_dts, 'r') as f:
        dts_content = f.read()
    
    nodes = simple_dts_parser(dts_content)
    
    # Generate files
    generate_header(nodes, output_dir / 'lq_generated.h')
    generate_source(nodes, output_dir / 'lq_generated.c')
    
    print(f"Generated {output_dir}/lq_generated.h")
    print(f"Generated {output_dir}/lq_generated.c")
    print(f"Found {len(nodes)} DTS nodes")
    
    # Auto-generate HIL tests
    generate_hil_tests(nodes, output_dir / 'lq_generated_test.dts')
    print(f"Generated {output_dir}/lq_generated_test.dts (HIL tests)")
    
    # Generate platform-specific hardware interface if requested
    if platform:
        generate_platform_hw(nodes, output_dir / 'lq_platform_hw.c', platform)
    else:
        print(f"\nTip: Add --platform=<name> to generate platform-specific ISRs")
        print(f"     Supported platforms: stm32, samd, esp32, nrf52, baremetal")

if __name__ == '__main__':
    main()
