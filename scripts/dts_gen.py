#!/usr/bin/env python3
"""
DTS to C Code Generator for Layered Queue Driver

This script parses devicetree files and generates:
- lq_generated.h: Forward declarations and extern definitions
- lq_generated.c: Complete engine struct initialization and ISR handlers

Usage:
    python3 scripts/dts_gen.py <input.dts> <output_dir>
"""

import sys
import re
from pathlib import Path

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
        
        # Check for boolean properties (no value)
        if 'signed' in content and 'signed' not in node.properties:
            node.properties['signed'] = True
        
        nodes.append(node)
    
    return nodes

def generate_header(nodes, output_path):
    """Generate lq_generated.h with declarations"""
    
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

#ifdef __cplusplus
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
    cyclic_outputs = []
    
    for node in nodes:
        if node.compatible == 'lq,engine':
            engine_node = node
        elif node.compatible.startswith('lq,hw-'):
            hw_inputs.append(node)
        elif node.compatible == 'lq,mid-merge':
            merges.append(node)
        elif node.compatible == 'lq,cyclic-output':
            cyclic_outputs.append(node)
    
    with open(output_path, 'w') as f:
        f.write("""/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 */

#include "lq_generated.h"
#include "lq_hw_input.h"
#include "lq_common.h"
#include "lq_event.h"
#include <string.h>

""")
        
        # Generate merge contexts
        if merges:
            f.write("/* Merge contexts */\n")
            f.write(f"static struct lq_merge_ctx g_merges[{len(merges)}] = {{\n")
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
                
                f.write(f"    [{i}] = {{\n")
                f.write(f"        .output_signal = {node.properties.get('output_signal_id', 0)},\n")
                f.write(f"        .input_signals = {{{', '.join(map(str, input_ids))}}},\n")
                f.write(f"        .num_inputs = {len(input_ids)},\n")
                f.write(f"        .voting_method = {vote_method},\n")
                f.write(f"        .tolerance = {node.properties.get('tolerance', 0)},\n")
                f.write(f"        .stale_us = {node.properties.get('stale_us', 0)},\n")
                f.write(f"    }},\n")
            f.write("};\n\n")
        
        # Generate cyclic output contexts
        if cyclic_outputs:
            f.write("/* Cyclic output contexts */\n")
            f.write(f"static struct lq_cyclic_ctx g_cyclic_outputs[{len(cyclic_outputs)}] = {{\n")
            for i, node in enumerate(cyclic_outputs):
                output_type_map = {
                    'can': 'LQ_OUTPUT_CAN',
                    'j1939': 'LQ_OUTPUT_J1939',
                    'canopen': 'LQ_OUTPUT_CANOPEN',
                    'gpio': 'LQ_OUTPUT_GPIO',
                    'uart': 'LQ_OUTPUT_UART',
                }
                output_type = output_type_map.get(node.properties.get('output_type', 'can'))
                
                f.write(f"    [{i}] = {{\n")
                f.write(f"        .signal_id = {node.properties.get('source_signal_id', 0)},\n")
                f.write(f"        .output_type = {output_type},\n")
                f.write(f"        .target_id = {node.properties.get('target_id', 0)},\n")
                f.write(f"        .period_us = {node.properties.get('period_us', 100000)},\n")
                f.write(f"        .next_deadline_us = {node.properties.get('deadline_offset_us', 0)},\n")
                f.write(f"        .priority = {node.properties.get('priority', 7)},\n")
                f.write(f"    }},\n")
            f.write("};\n\n")
        
        # Generate engine instance
        f.write("/* Engine instance */\n")
        f.write("struct lq_engine g_lq_engine = {\n")
        if engine_node:
            f.write(f"    .num_signals = {engine_node.properties.get('max_signals', 32)},\n")
        else:
            f.write(f"    .num_signals = 32,\n")
        f.write(f"    .num_merges = {len(merges)},\n")
        f.write(f"    .num_cyclic_outputs = {len(cyclic_outputs)},\n")
        f.write("    .signals = {0},\n")
        if merges:
            f.write("    .merges = g_merges,\n")
        if cyclic_outputs:
            f.write("    .cyclic_outputs = g_cyclic_outputs,\n")
        f.write("};\n\n")
        
        # Generate ISR handlers for hardware inputs
        for node in hw_inputs:
            signal_id = node.properties.get('signal_id', 0)
            
            if node.compatible == 'lq,hw-adc-input':
                f.write(f"/* ADC ISR for {node.label} */\n")
                f.write(f"void lq_adc_isr_{node.label}(uint16_t value) {{\n")
                f.write(f"    lq_hw_push({signal_id}, (int32_t)value, lq_platform_get_time_us());\n")
                f.write(f"}}\n\n")
            
            elif node.compatible == 'lq,hw-spi-input':
                f.write(f"/* SPI ISR for {node.label} */\n")
                f.write(f"void lq_spi_isr_{node.label}(int32_t value) {{\n")
                f.write(f"    lq_hw_push({signal_id}, value, lq_platform_get_time_us());\n")
                f.write(f"}}\n\n")
        
        # Generate init function
        f.write("/* Initialization */\n")
        f.write("int lq_generated_init(void) {\n")
        f.write("    /* Hardware input layer */\n")
        f.write("    int ret = lq_hw_input_init();\n")
        f.write("    if (ret != 0) return ret;\n")
        f.write("    \n")
        f.write("    /* TODO: Configure ADC/SPI/Sensor triggers here */\n")
        f.write("    \n")
        f.write("    return 0;\n")
        f.write("}\n")

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.dts> <output_dir>")
        sys.exit(1)
    
    input_dts = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
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

if __name__ == '__main__':
    main()
