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
            prop_name = prop_match.group(1)
            prop_value = prop_match.group(2)
            
            # Skip 'compatible' as we already have it
            if prop_name == 'compatible':
                continue
            
            node.properties[prop_name] = parse_property_value(prop_value)
        
        # Extract boolean flags (properties without values)
        flag_pattern = r'\b(enable-[\w-]+|has-[\w-]+)\s*;'
        for flag_match in re.finditer(flag_pattern, content):
            flag_name = flag_match.group(1)
            node.properties[flag_name] = True
        
        nodes.append(node)
    
    return nodes


def expand_eds_references(input_dts_path, output_dts_path, signals_header_path=None):
    """Find CANopen nodes with 'eds' property and expand them"""
    import os
    from pathlib import Path
    import sys
    
    # Add scripts directory to path for imports
    sys.path.insert(0, str(Path(__file__).parent))
    
    # Read the input DTS
    with open(input_dts_path, 'r') as f:
        dts_content = f.read()
    
    # Find canopen nodes with eds property
    # Pattern: label: canopen-device@N { ... eds = "file.eds"; ... }
    canopen_pattern = r'(\w+):\s*(canopen-device@\d+)\s*\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}'
    
    expanded_content = dts_content
    all_signal_ids = []
    
    for match in re.finditer(canopen_pattern, dts_content):
        label = match.group(1)
        node_decl = match.group(2)
        node_content = match.group(3)
        full_node = match.group(0)
        
        # Check if this node has an 'eds' property
        eds_match = re.search(r'eds\s*=\s*"([^"]+)"', node_content)
        if not eds_match:
            continue
        
        eds_file = eds_match.group(1)
        
        # Resolve EDS path relative to DTS directory
        dts_dir = Path(input_dts_path).parent
        eds_path = dts_dir / eds_file
        
        if not eds_path.exists():
            print(f"Warning: EDS file not found: {eds_path}")
            continue
        
        # Extract node-id override if present
        node_id_match = re.search(r'node-id\s*=\s*<(\d+)>', node_content)
        node_id = int(node_id_match.group(1)) if node_id_match else None
        
        try:
            from canopen_eds_parser import parse_eds_file, generate_device_tree_content, generate_signal_header
            
            # Parse EDS
            eds_data = parse_eds_file(str(eds_path))
            
            # Override node-id if specified in DTS
            if node_id is not None:
                eds_data['node_id'] = node_id
            
            # Collect signal IDs for header generation
            all_signal_ids.extend(eds_data.get('rpdos', []))
            all_signal_ids.extend(eds_data.get('tpdos', []))
            
            # Get address from node declaration
            address = node_decl.split('@')[1]
            
            # Generate full canopen node content (with proper indentation)
            expanded_node_content = generate_device_tree_content(eds_data, label, address)
            
            # Replace the stub node with expanded version
            expanded_content = expanded_content.replace(full_node, expanded_node_content)
            
            print(f"Expanded CANopen node '{label}' from {eds_file}")
            
        except ImportError as e:
            print(f"Warning: Could not import EDS parser: {e}")
            continue
    
    # Write expanded DTS
    with open(output_dts_path, 'w') as f:
        f.write(expanded_content)
    
    # Generate signal header if requested
    if signals_header_path and all_signal_ids:
        from canopen_eds_parser import generate_signal_header
        with open(signals_header_path, 'w') as f:
            f.write(generate_signal_header(all_signal_ids))
    
    return expanded_content


def generate_device_tree_content(eds_data, label, address):
    """Generate complete CANopen DTS node content from EDS data"""
    lines = []
    
    lines.append(f"{label}: canopen-device@{address} {{")
    lines.append(f'    compatible = "lq,protocol-canopen";')
    lines.append(f'    node-id = <{eds_data["node_id"]}>;')
    lines.append(f'    label = "{eds_data["device_name"]}";')
    lines.append(f'')
    lines.append(f'    /* Auto-generated from EDS file */')
    
    # Add TPDOs
    for idx, tpdo in enumerate(eds_data.get('tpdos', [])):
        lines.append(f'')
        lines.append(f'    tpdo{idx}: tpdo@{idx} {{')
        lines.append(f'        cob-id = <{tpdo["cob_id"]}>;')
        
        for map_idx, mapping in enumerate(tpdo['mappings']):
            lines.append(f'')
            lines.append(f'        mapping@{map_idx} {{')
            lines.append(f'            index = <{mapping["index"]}>;')
            lines.append(f'            subindex = <{mapping["subindex"]}>;')
            lines.append(f'            length = <{mapping["length"]}>;')
            lines.append(f'            signal-id = <{mapping["signal_id"]}>;')
            lines.append(f'        }};')
        
        lines.append(f'    }};')
    
    # Add RPDOs
    for idx, rpdo in enumerate(eds_data.get('rpdos', [])):
        lines.append(f'')
        lines.append(f'    rpdo{idx}: rpdo@{idx} {{')
        lines.append(f'        cob-id = <{rpdo["cob_id"]}>;')
        
        for map_idx, mapping in enumerate(rpdo['mappings']):
            lines.append(f'')
            lines.append(f'        mapping@{map_idx} {{')
            lines.append(f'            index = <{mapping["index"]}>;')
            lines.append(f'            subindex = <{mapping["subindex"]}>;')
            lines.append(f'            length = <{mapping["length"]}>;')
            lines.append(f'            signal-id = <{mapping["signal_id"]}>;')
            lines.append(f'        }};')
        
        lines.append(f'    }};')
    
    lines.append(f'}};')
    
    return '\n'.join(lines)


def parse_property_value(value):
    """Parse DTS property value - handle <>, "", arrays, phandles"""
    value = value.strip().rstrip(';')
    
    # Phandle reference: <&sensor> or <&sensor1 &sensor2>
    if value.startswith('<') and '&' in value:
        inner = value[1:-1].strip()
        # Multiple phandles: <&s1 &s2 &s3>
        if ' ' in inner:
            refs = [ref.strip()[1:] if ref.strip().startswith('&') else ref.strip() 
                   for ref in inner.split()]
            return refs
        # Single phandle: <&sensor>
        return inner[1:] if inner.startswith('&') else inner
    
    # Array of integers: <1 2 3>
    if value.startswith('<') and value.endswith('>'):
        inner = value[1:-1].strip()
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

def resolve_phandles_and_assign_ids(nodes):
    """
    Resolve phandle references to signal IDs and auto-assign IDs.
    
    Unified property names:
    - source: single input (scale, remap, PID)
    - sources: multiple inputs (merge/voter)
    - input: monitoring input (fault-monitor)
    - output: explicit output signal (optional, auto-assigned if not specified)
    
    Backward compatibility:
    - signal-id, source-signal, input-signal, etc. still work
    """
    # Build label->node map
    label_map = {node.label: node for node in nodes}
    
    # Initialize signal_id attribute for all nodes
    for node in nodes:
        node.signal_id = None
    
    # Auto-assign signal IDs in order
    signal_id = 0
    for node in nodes:
        # Skip if already has explicit signal-id
        if 'signal_id' in node.properties:
            node.signal_id = node.properties['signal_id']
            signal_id = max(signal_id, node.signal_id + 1)
        # Hardware inputs and processing nodes get signal IDs
        elif (node.compatible.startswith('lq,hw-') or 
              node.compatible in ['lq,scale', 'lq,remap', 'lq,pid', 'lq,mid-merge']):
            node.signal_id = signal_id
            node.properties['signal_id'] = signal_id
            signal_id += 1
        # Fault monitors create output signals
        elif node.compatible == 'lq,fault-monitor':
            if 'fault_output_signal_id' not in node.properties:
                node.properties['fault_output_signal_id'] = signal_id
                # Also set signal_id for the fault monitor node itself
                node.signal_id = signal_id
                signal_id += 1
    
    # Resolve phandle references
    for node in nodes:
        # Unified: source (single input)
        if 'source' in node.properties:
            ref = node.properties['source']
            if isinstance(ref, str) and ref in label_map:
                if label_map[ref].signal_id is not None:
                    node.properties['source_signal'] = label_map[ref].signal_id
        # Backward compat: source-signal
        elif 'source_signal' in node.properties:
            ref = node.properties['source_signal']
            if isinstance(ref, str) and ref in label_map:
                if label_map[ref].signal_id is not None:
                    node.properties['source_signal'] = label_map[ref].signal_id
        
        # Unified: sources (multiple inputs)
        if 'sources' in node.properties:
            refs = node.properties['sources']
            if not isinstance(refs, list):
                refs = [refs]
            ids = []
            for ref in refs:
                if isinstance(ref, str) and ref in label_map:
                    if label_map[ref].signal_id is not None:
                        ids.append(label_map[ref].signal_id)
                elif isinstance(ref, int):
                    ids.append(ref)
            node.properties['input_signal_ids'] = ids
        # Backward compat: input-signal-ids
        elif 'input_signal_ids' in node.properties:
            refs = node.properties['input_signal_ids']
            if not isinstance(refs, list):
                refs = [refs]
            ids = []
            for ref in refs:
                if isinstance(ref, str) and ref in label_map:
                    if label_map[ref].signal_id is not None:
                        ids.append(label_map[ref].signal_id)
                elif isinstance(ref, int):
                    ids.append(ref)
            node.properties['input_signal_ids'] = ids
        
        # Unified: input (fault monitor, etc)
        if 'input' in node.properties:
            ref = node.properties['input']
            if isinstance(ref, str) and ref in label_map:
                if label_map[ref].signal_id is not None:
                    node.properties['input_signal'] = label_map[ref].signal_id
        # Backward compat: input-signal
        elif 'input_signal' in node.properties:
            ref = node.properties['input_signal']
            if isinstance(ref, str) and ref in label_map:
                if label_map[ref].signal_id is not None:
                    node.properties['input_signal'] = label_map[ref].signal_id
        
        # Unified: output (explicit output signal)
        if 'output' in node.properties:
            ref = node.properties['output']
            if isinstance(ref, str) and ref in label_map:
                if label_map[ref].signal_id is not None:
                    node.properties['output_signal'] = label_map[ref].signal_id
        # Backward compat: output-signal
        elif 'output_signal' in node.properties:
            ref = node.properties['output_signal']
            if isinstance(ref, str) and ref in label_map:
                if label_map[ref].signal_id is not None:
                    node.properties['output_signal'] = label_map[ref].signal_id
    
    return nodes

def calculate_resource_counts(nodes):
    """
    Analyze devicetree nodes and calculate exact resource requirements.
    Returns a dict with all counts needed for configuration.
    """
    counts = {
        'num_signals': 0,
        'num_hw_inputs': 0,
        'num_scales': 0,
        'num_remaps': 0,
        'num_merges': 0,
        'num_fault_monitors': 0,
        'num_cyclic_outputs': 0,
        'num_pid_controllers': 0,
        'num_verified_outputs': 0,
        'max_merge_inputs': 0,
        'max_output_events': 0,
        'hw_ringbuffer_size': 128,  # Default, can be overridden by engine node
    }
    
    # Check for engine node with overrides
    engine_nodes = [n for n in nodes if n.compatible == 'lq,engine']
    if engine_nodes:
        eng = engine_nodes[0]
        if 'hw_ringbuffer_size' in eng.properties:
            counts['hw_ringbuffer_size'] = eng.properties['hw_ringbuffer_size']
    
    # Count nodes by type
    for node in nodes:
        if node.compatible.startswith('lq,hw-'):
            counts['num_hw_inputs'] += 1
        elif node.compatible == 'lq-scale' or node.compatible == 'lq,scale':
            counts['num_scales'] += 1
        elif node.compatible == 'lq,remap':
            counts['num_remaps'] += 1
        elif node.compatible == 'lq,mid-merge':
            counts['num_merges'] += 1
            # Track max merge input count
            input_ids = node.properties.get('input_signal_ids', [])
            if isinstance(input_ids, int):
                input_ids = [input_ids]
            counts['max_merge_inputs'] = max(counts['max_merge_inputs'], len(input_ids))
        elif node.compatible == 'lq,fault-monitor':
            counts['num_fault_monitors'] += 1
        elif node.compatible == 'lq,cyclic-output':
            counts['num_cyclic_outputs'] += 1
        elif node.compatible == 'lq,pid' or node.compatible == 'lq,pid-controller':
            counts['num_pid_controllers'] += 1
        elif node.compatible == 'lq,verified-output':
            counts['num_verified_outputs'] += 1
    
    # Calculate total signal count (max signal ID + 1)
    max_signal_id = 0
    for node in nodes:
        if hasattr(node, 'signal_id') and node.signal_id is not None:
            max_signal_id = max(max_signal_id, node.signal_id)
        # Also check explicit signal IDs in properties
        for prop in ['signal_id', 'output_signal_id', 'fault_output_signal_id']:
            if prop in node.properties:
                max_signal_id = max(max_signal_id, node.properties[prop])
    
    counts['num_signals'] = max_signal_id + 1
    
    # Estimate max output events (cyclic outputs * 2 for safety margin)
    counts['max_output_events'] = max(counts['num_cyclic_outputs'] * 2, 16)
    
    return counts


def generate_config_header(counts, output_path):
    """Generate lq_config.h with exact resource counts from devicetree"""
    
    # Calculate memory savings vs default Kconfig values
    default_signals = 32
    default_cyclic = 16
    default_merges = 8
    
    signal_saving_pct = int((1 - counts['num_signals'] / default_signals) * 100) if counts['num_signals'] < default_signals else 0
    
    with open(output_path, 'w') as f:
        f.write("""/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * This file defines exact resource counts based on your devicetree.
 * No manual Kconfig tuning needed - memory automatically optimized!
 * 
 * Signal array memory: {num_signals} signals (vs default {default_signals})
 * Savings: ~{signal_saving_pct}% reduction in static allocation
 */

#ifndef LQ_CONFIG_H_
#define LQ_CONFIG_H_

/* Signal counts - auto-calculated from DTS */
#define LQ_MAX_SIGNALS              {num_signals}

/* Driver instance counts - exact counts from DTS */
#define LQ_MAX_HW_INPUTS            {num_hw_inputs}
#define LQ_MAX_SCALES               {num_scales}
#define LQ_MAX_REMAPS               {num_remaps}
#define LQ_MAX_MERGES               {num_merges}
#define LQ_MAX_FAULT_MONITORS       {num_fault_monitors}
#define LQ_MAX_CYCLIC_OUTPUTS       {num_cyclic_outputs}
#define LQ_MAX_PID_CONTROLLERS      {num_pid_controllers}
#define LQ_MAX_VERIFIED_OUTPUTS     {num_verified_outputs}

/* Buffer sizes - calculated from actual usage */
#define LQ_MAX_MERGE_INPUTS         {max_merge_inputs}
#define LQ_MAX_OUTPUT_EVENTS        {max_output_events}
#define LQ_HW_RINGBUFFER_SIZE       {hw_ringbuffer_size}

/* DTS generation metadata */
#define LQ_CONFIG_FROM_DTS          1
#define LQ_CONFIG_SIGNAL_COUNT      {num_signals}

#endif /* LQ_CONFIG_H_ */
""".format(
            num_signals=counts['num_signals'],
            default_signals=default_signals,
            signal_saving_pct=signal_saving_pct,
            num_hw_inputs=counts['num_hw_inputs'],
            num_scales=counts['num_scales'],
            num_remaps=counts['num_remaps'],
            num_merges=counts['num_merges'],
            num_fault_monitors=counts['num_fault_monitors'],
            num_cyclic_outputs=counts['num_cyclic_outputs'],
            num_pid_controllers=counts['num_pid_controllers'],
            num_verified_outputs=counts['num_verified_outputs'],
            max_merge_inputs=counts['max_merge_inputs'],
            max_output_events=counts['max_output_events'],
            hw_ringbuffer_size=counts['hw_ringbuffer_size']
        ))


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

/* Output event dispatcher */
void lq_generated_dispatch_outputs(void);

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
    crosscheck_nodes = []
    
    for node in nodes:
        if node.compatible == 'lq,engine':
            engine_node = node
        # Generalized input/output support
        elif node.compatible in ['lq,input', 'lq,output']:
            # For now, treat as hardware input/output (CAN only)
            # If device property references a CAN device, treat as CAN input/output
            dev = node.properties.get('device')
            if dev and (isinstance(dev, str) and 'can' in dev.lower()):
                # Extract device index from device name (e.g., can0=0, can1=1, can2=2)
                import re
                match = re.search(r'can(\d+)', dev.lower())
                device_index = int(match.group(1)) if match else 0
                node.properties['device_index'] = device_index

                if node.compatible == 'lq,input':
                    node.compatible = 'lq,hw-can-input'
                    hw_inputs.append(node)
                elif node.compatible == 'lq,output':
                    node.compatible = 'lq,cyclic-output'
                    cyclic_outputs.append(node)
            # TODO: Add support for ADC, UART, etc. in future
        elif node.compatible.startswith('lq,hw-'):
            hw_inputs.append(node)
        elif node.compatible == 'lq,mid-merge':
            merges.append(node)
        elif node.compatible == 'lq,fault-monitor':
            fault_monitors.append(node)
        elif node.compatible == 'lq,cyclic-output':
            cyclic_outputs.append(node)
        elif node.compatible == 'lq,event-crosscheck':
            crosscheck_nodes.append(node)
    
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
    
    # Determine which output types are used for conditional includes
    output_types_used = set()
    for node in cyclic_outputs:
        output_type = node.properties.get('output_type', 'can')
        output_types_used.add(output_type)
    
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
""")
        
        # Add protocol-specific includes based on what's used
        if 'j1939' in output_types_used:
            f.write("#include \"lq_j1939.h\"\n")
        if 'canopen' in output_types_used:
            f.write("#include \"lq_canopen.h\"\n")
        
        # Add crosscheck include if enabled
        if crosscheck_nodes:
            f.write("#include \"lq_event_crosscheck.h\"\n")
        
        # Add platform includes if any CAN-based output is used
        if any(t in output_types_used for t in ['j1939', 'canopen', 'can']):
            f.write("#include \"lq_platform.h\"  /* For lq_can_send */\n")
        if 'gpio' in output_types_used:
            f.write("#include \"lq_platform.h\"  /* For lq_gpio_set */\n")
        if 'uart' in output_types_used:
            f.write("#include \"lq_platform.h\"  /* For lq_uart_send */\n")
        if 'spi' in output_types_used:
            f.write("#include \"lq_platform.h\"  /* For lq_spi_send */\n")
        if 'i2c' in output_types_used:
            f.write("#include \"lq_platform.h\"  /* For lq_i2c_write */\n")
        if 'pwm' in output_types_used:
            f.write("#include \"lq_platform.h\"  /* For lq_pwm_set */\n")
        if 'dac' in output_types_used:
            f.write("#include \"lq_platform.h\"  /* For lq_dac_write */\n")
        if 'modbus' in output_types_used:
            f.write("#include \"lq_platform.h\"  /* For lq_modbus_write */\n")
        
        f.write("#include <stdlib.h>\n")
        f.write("#include <string.h>\n")
        f.write("\n")
        
        # Platform function declarations (portable approach)
        # Note: Implementations can be provided by:
        # 1. User's platform-specific code
        # 2. Linking with lq_platform_stubs.c (provides default no-op implementations)
        # 3. Weak symbols on GNU toolchains (see lq_platform_stubs.c)
        f.write("/* Platform function declarations - implement these in your platform code\n")
        f.write(" * or link with lq_platform_stubs.c for default no-op implementations */\n")
        
        if any(t in output_types_used for t in ['j1939', 'canopen', 'can']):
            f.write("extern int lq_can_send(uint8_t device_index, uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len);\n")
        
        if 'gpio' in output_types_used:
            f.write("extern int lq_gpio_set(uint8_t pin, bool state);\n")
        
        if 'uart' in output_types_used:
            f.write("extern int lq_uart_send(const uint8_t *data, size_t len);\n")
        
        if 'spi' in output_types_used:
            f.write("extern int lq_spi_send(uint8_t device, const uint8_t *data, size_t len);\n")
        
        if 'i2c' in output_types_used:
            f.write("extern int lq_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);\n")
        
        if 'pwm' in output_types_used:
            f.write("extern int lq_pwm_set(uint8_t channel, uint32_t duty_cycle);\n")
        
        if 'dac' in output_types_used:
            f.write("extern int lq_dac_write(uint8_t channel, uint16_t value);\n")
        
        if 'modbus' in output_types_used:
            f.write("extern int lq_modbus_write(uint8_t slave_id, uint16_t reg, uint16_t value);\n")
        
        f.write("\n")
        
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
                f.write(f"            .output_signal = {node.signal_id},\n")
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
                f.write(f"            .device_index = {node.properties.get('device_index', 0)},\n")
                f.write(f"            .source_signal = {node.properties.get('source_signal_id', 0)},\n")
                f.write(f"            .period_us = {node.properties.get('period_us', 100000)},\n")
                f.write(f"            .next_deadline = {node.properties.get('deadline_offset_us', 0)},\n")
                f.write(f"            .flags = 0,\n")
                f.write(f"            .enabled = true,\n")
                f.write(f"        }},\n")
            f.write("    },\n")
        
        f.write("};\n\n")
        
        # Generate crosscheck context if enabled
        if crosscheck_nodes:
            crosscheck = crosscheck_nodes[0]  # Use first crosscheck node
            f.write("/* Event crosscheck context (dual-channel safety) */\n")
            f.write("static struct lq_crosscheck_ctx g_crosscheck_ctx;\n\n")
        
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
        f.write("        lq_hil_init(LQ_HIL_MODE_DISABLED, getenv(\"LQ_HIL_MODE\"), 0);\n")
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
        
        # Add crosscheck initialization if enabled
        if crosscheck_nodes:
            crosscheck = crosscheck_nodes[0]
            uart_id = crosscheck.properties.get('uart_id', 1)
            timeout_ms = crosscheck.properties.get('timeout_ms', 50)
            fail_gpio = crosscheck.properties.get('fail_gpio', 25)
            
            f.write("    /* Initialize event crosscheck (dual-channel safety) */\n")
            f.write(f"    ret = lq_crosscheck_init(&g_crosscheck_ctx, {uart_id}, {timeout_ms}, {fail_gpio});\n")
            f.write("    if (ret != 0) return ret;\n")
            f.write("    \n")
        
        f.write("    /* Platform-specific peripheral init */\n")
        f.write("    #ifdef LQ_PLATFORM_INIT\n")
        f.write("    lq_platform_peripherals_init();\n")
        f.write("    #endif\n")
        f.write("    \n")
        f.write("    return 0;\n")
        f.write("}\n\n")
        
        # Generate output dispatch function
        f.write("/* Output event dispatcher */\n")
        f.write("void lq_generated_dispatch_outputs(void) {\n")
        
        # Add crosscheck send hook if enabled
        if crosscheck_nodes:
            f.write("    /* Send events to other MCU for dual-channel verification */\n")
            f.write("    for (size_t i = 0; i < g_lq_engine.out_event_count; i++) {\n")
            f.write("        lq_crosscheck_send_event(&g_crosscheck_ctx, &g_lq_engine.out_events[i]);\n")
            f.write("    }\n")
            f.write("    \n")
        
        f.write("    /* Dispatch output events to appropriate protocol drivers/hardware */\n")
        f.write("    for (size_t i = 0; i < g_lq_engine.out_event_count; i++) {\n")
        f.write("        struct lq_output_event *evt = &g_lq_engine.out_events[i];\n")
        f.write("        \n")
        f.write("        switch (evt->type) {\n")
        
        # Determine which output types are actually used
        output_types_used = set()
        for node in cyclic_outputs:
            output_type = node.properties.get('output_type', 'can')
            output_types_used.add(output_type)
        
        # Generate dispatch cases for each used output type
        if 'j1939' in output_types_used:
            f.write("            case LQ_OUTPUT_J1939: {\n")
            f.write("                /* J1939 output: encode value and send via CAN */\n")
            f.write("                uint8_t data[8] = {0};\n")
            f.write("                data[0] = (uint8_t)(evt->value & 0xFF);\n")
            f.write("                data[1] = (uint8_t)((evt->value >> 8) & 0xFF);\n")
            f.write("                data[2] = (uint8_t)((evt->value >> 16) & 0xFF);\n")
            f.write("                data[3] = (uint8_t)((evt->value >> 24) & 0xFF);\n")
            f.write("                \n")
            f.write("                /* Build CAN ID from PGN (target_id) */\n")
            f.write("                uint32_t can_id = lq_j1939_build_id_from_pgn(evt->target_id, 6, 0);\n")
            f.write("                lq_can_send(evt->device_index, can_id, true, data, 8);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'canopen' in output_types_used:
            f.write("            case LQ_OUTPUT_CANOPEN: {\n")
            f.write("                /* CANopen output: encode PDO and send */\n")
            f.write("                uint8_t data[8] = {0};\n")
            f.write("                data[0] = (uint8_t)(evt->value & 0xFF);\n")
            f.write("                data[1] = (uint8_t)((evt->value >> 8) & 0xFF);\n")
            f.write("                data[2] = (uint8_t)((evt->value >> 16) & 0xFF);\n")
            f.write("                data[3] = (uint8_t)((evt->value >> 24) & 0xFF);\n")
            f.write("                \n")
            f.write("                /* target_id is COB-ID */\n")
            f.write("                lq_can_send(evt->device_index, evt->target_id, false, data, 4);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'spi' in output_types_used:
            f.write("            case LQ_OUTPUT_SPI: {\n")
            f.write("                /* SPI output: target_id is device/CS, value is data */\n")
            f.write("                uint8_t data[4];\n")
            f.write("                data[0] = (uint8_t)(evt->value & 0xFF);\n")
            f.write("                data[1] = (uint8_t)((evt->value >> 8) & 0xFF);\n")
            f.write("                data[2] = (uint8_t)((evt->value >> 16) & 0xFF);\n")
            f.write("                data[3] = (uint8_t)((evt->value >> 24) & 0xFF);\n")
            f.write("                lq_spi_send((uint8_t)evt->target_id, data, 4);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'i2c' in output_types_used:
            f.write("            case LQ_OUTPUT_I2C: {\n")
            f.write("                /* I2C output: target_id bits[15:8]=addr, bits[7:0]=register */\n")
            f.write("                uint8_t addr = (uint8_t)((evt->target_id >> 8) & 0xFF);\n")
            f.write("                uint8_t reg = (uint8_t)(evt->target_id & 0xFF);\n")
            f.write("                uint8_t data[4];\n")
            f.write("                data[0] = (uint8_t)(evt->value & 0xFF);\n")
            f.write("                data[1] = (uint8_t)((evt->value >> 8) & 0xFF);\n")
            f.write("                data[2] = (uint8_t)((evt->value >> 16) & 0xFF);\n")
            f.write("                data[3] = (uint8_t)((evt->value >> 24) & 0xFF);\n")
            f.write("                lq_i2c_write(addr, reg, data, 4);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'pwm' in output_types_used:
            f.write("            case LQ_OUTPUT_PWM: {\n")
            f.write("                /* PWM output: target_id is channel, value is duty cycle */\n")
            f.write("                lq_pwm_set((uint8_t)evt->target_id, (uint32_t)evt->value);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'dac' in output_types_used:
            f.write("            case LQ_OUTPUT_DAC: {\n")
            f.write("                /* DAC output: target_id is channel, value is analog level */\n")
            f.write("                lq_dac_write((uint8_t)evt->target_id, (uint16_t)evt->value);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'modbus' in output_types_used:
            f.write("            case LQ_OUTPUT_MODBUS: {\n")
            f.write("                /* Modbus output: target_id bits[23:16]=slave, bits[15:0]=register */\n")
            f.write("                uint8_t slave = (uint8_t)((evt->target_id >> 16) & 0xFF);\n")
            f.write("                uint16_t reg = (uint16_t)(evt->target_id & 0xFFFF);\n")
            f.write("                lq_modbus_write(slave, reg, (uint16_t)evt->value);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
            f.write("                data[0] = (uint8_t)(evt->value & 0xFF);\n")
            f.write("                data[1] = (uint8_t)((evt->value >> 8) & 0xFF);\n")
            f.write("                data[2] = (uint8_t)((evt->value >> 16) & 0xFF);\n")
            f.write("                data[3] = (uint8_t)((evt->value >> 24) & 0xFF);\n")
            f.write("                \n")
            f.write("                /* target_id is COB-ID */\n")
            f.write("                lq_can_send(evt->device_index, evt->target_id, false, data, 4);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'can' in output_types_used:
            f.write("            case LQ_OUTPUT_CAN: {\n")
            f.write("                /* Raw CAN output */\n")
            f.write("                uint8_t data[8] = {0};\n")
            f.write("                data[0] = (uint8_t)(evt->value & 0xFF);\n")
            f.write("                data[1] = (uint8_t)((evt->value >> 8) & 0xFF);\n")
            f.write("                data[2] = (uint8_t)((evt->value >> 16) & 0xFF);\n")
            f.write("                data[3] = (uint8_t)((evt->value >> 24) & 0xFF);\n")
            f.write("                \n")
            f.write("                bool extended = (evt->flags & 1) != 0;\n")
            f.write("                lq_can_send(evt->device_index, evt->target_id, extended, data, 8);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'gpio' in output_types_used:
            f.write("            case LQ_OUTPUT_GPIO: {\n")
            f.write("                /* GPIO output: target_id is pin number */\n")
            f.write("                lq_gpio_set((uint8_t)evt->target_id, evt->value != 0);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        if 'uart' in output_types_used:
            f.write("            case LQ_OUTPUT_UART: {\n")
            f.write("                /* UART output: send as ASCII string */\n")
            f.write("                char buf[32];\n")
            f.write("                int len = snprintf(buf, sizeof(buf), \"%d\\n\", evt->value);\n")
            f.write("                lq_uart_send((uint8_t*)buf, len);\n")
            f.write("                break;\n")
            f.write("            }\n")
        
        f.write("            default:\n")
        f.write("                /* Unknown output type - ignore */\n")
        f.write("                break;\n")
        f.write("        }\n")
        f.write("    }\n")
        f.write("}\n")


def generate_hil_tests(nodes, output_path):
    """Generate HIL test devicetree for native platform testing"""
    generate_hil_tests_impl(nodes, output_path)


def generate_main(nodes, output_path, platform='baremetal'):
    """Generate simple platform-agnostic main.c - platform layer handles RTOS details"""
    
    template = """/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 * 
 * Platform layer (lq_platform_*.c) handles:
 * - FreeRTOS: Creates task and starts scheduler
 * - Zephyr: Creates thread
 * - Native/Bare metal: Runs infinite loop
 */

#include "lq_engine.h"
#include "lq_generated.h"
#include "lq_platform.h"
#include <stdio.h>

int main(void)
{
    printf("Layered Queue Application\\n");
    printf("Signals: %u, Merges: %u, Cyclic: %u\\n",
           g_lq_engine.num_signals,
           g_lq_engine.num_merges,
           g_lq_engine.num_cyclic_outputs);
    
    /* Initialize engine and platform */
    int ret = lq_generated_init();
    if (ret != 0) {
        printf("ERROR: Initialization failed: %d\\n", ret);
        return ret;
    }
    
    printf("Initialization complete\\n");
    
    /* Start engine - platform layer handles tasks/threads/loop */
    return lq_engine_run();
}
"""
    
    with open(output_path, 'w') as f:
        f.write(template)


def generate_hil_tests_impl(nodes, output_path):
    """Auto-generate HIL tests from system definition"""
    
    # Collect all inputs
    adc_sources = [n for n in nodes if 'adc' in n.compatible]
    spi_sources = [n for n in nodes if 'spi' in n.compatible]
    can_sources = [n for n in nodes if 'can' in n.compatible]
    merge_nodes = [n for n in nodes if 'merge' in n.compatible or 'voter' in n.compatible]
    fault_monitors = [n for n in nodes if 'fault-monitor' in n.compatible]
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
        
        # Expect output based on actual output type
        if output_nodes:
            output = output_nodes[0]
            output_type = output.properties.get('output_type', 'can')
            
            if output_type == 'gpio':
                pin = output.properties.get('target_id', 0)
                timeout = output.properties.get('expected_response_ms', 200)
                # GPIO output - expect it to go high when signal is active
                f.write(f"            step@{step} {{\n")
                f.write(f"                action = \"wait-gpio-high\";\n")
                f.write(f"                pin = <{pin}>;\n")
                f.write(f"                timeout-ms = <{timeout}>;\n")
                f.write(f"            }};\n")
            elif output_type == 'can' or output_type == 'canopen':
                # For CANopen, use cob-id; for regular CAN, use pgn
                if output_type == 'canopen':
                    can_id = output.properties.get('cob_id', 0x180)
                    timeout = output.properties.get('expected_response_ms', 1500)
                    f.write(f"            step@{step} {{\n")
                    f.write(f"                action = \"expect-can\";\n")
                    f.write(f"                can-id = <{can_id}>;\n")
                    f.write(f"                timeout-ms = <{timeout}>;\n")
                    f.write(f"            }};\n")
                else:
                    pgn = output.properties.get('pgn', 61444)
                    timeout = output.properties.get('expected_response_ms', 200)
                    f.write(f"            step@{step} {{\n")
                    f.write(f"                action = \"expect-can\";\n")
                    f.write(f"                pgn = <{pgn}>;\n")
                    f.write(f"                timeout-ms = <{timeout}>;\n")
                    f.write(f"            }};\n")
            elif output_type == 'pwm':
                channel = output.properties.get('target_id', 0)
                f.write(f"            step@{step} {{\n")
                f.write(f"                action = \"measure-pwm\";\n")
                f.write(f"                channel = <{channel}>;\n")
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
            
            # Verify merged output based on actual output type
            if output_nodes:
                output = output_nodes[0]
                output_type = output.properties.get('output_type', 'can')
                
                if output_type == 'gpio':
                    pin = output.properties.get('target_id', 0)
                    f.write(f"            step@{step} {{\n")
                    f.write(f"                action = \"wait-gpio-high\";\n")
                    f.write(f"                pin = <{pin}>;\n")
                    f.write(f"                timeout-ms = <500>;\n")
                    f.write(f"            }};\n")
                elif output_type == 'can' or output_type == 'canopen':
                    if output_type == 'canopen':
                        can_id = output.properties.get('cob_id', 0x180)
                        f.write(f"            step@{step} {{\n")
                        f.write(f"                action = \"expect-can\";\n")
                        f.write(f"                can-id = <{can_id}>;\n")
                        f.write(f"                timeout-ms = <1500>;\n")
                        f.write(f"            }};\n")
                    else:
                        pgn = output.properties.get('pgn', 61444)
                        f.write(f"            step@{step} {{\n")
                        f.write(f"                action = \"expect-can\";\n")
                        f.write(f"                pgn = <{pgn}>;\n")
                        f.write(f"                timeout-ms = <200>;\n")
                        f.write(f"            }};\n")
            
            f.write("        };\n")
            f.write("    };\n\n")
        
        # Test 3: Fault condition triggering
        if fault_monitors and adc_sources and output_nodes:
            fault = fault_monitors[0]
            adc = adc_sources[0]
            output = output_nodes[0]
            channel = adc.properties.get('channel', 0)
            output_type = output.properties.get('output_type', 'can')
            
            # Get fault threshold and response time from monitor
            max_value = fault.properties.get('max_value', 5000)
            fault_timeout = fault.properties.get('expected_response_ms', 50)
            fault_test_value = max_value + 1000  # Above threshold
            
            f.write("    hil-test-fault-trigger {\n")
            f.write("        compatible = \"lq,hil-test\";\n")
            f.write("        description = \"Test fault detection triggers output\";\n")
            f.write("        timeout-ms = <2000>;\n")
            f.write("        \n")
            f.write("        sequence {\n")
            f.write("            step@0 {\n")
            f.write("                action = \"inject-adc\";\n")
            f.write(f"                channel = <{channel}>;\n")
            f.write(f"                value = <{fault_test_value}>;  /* Above max threshold */\n")
            f.write("            };\n")
            
            if output_type == 'gpio':
                pin = output.properties.get('target_id', 0)
                f.write("            step@1 {\n")
                f.write("                action = \"wait-gpio-high\";\n")
                f.write(f"                pin = <{pin}>;\n")
                f.write(f"                timeout-ms = <{fault_timeout}>;\n")
                f.write("            };\n")
            elif output_type == 'can' or output_type == 'canopen':
                # For CANopen faults, still check for DM1
                f.write("            step@1 {\n")
                f.write("                action = \"expect-can\";\n")
                f.write("                pgn = <65226>;  /* DM1 diagnostic message */\n")
                f.write(f"                timeout-ms = <{fault_timeout}>;\n")
                f.write("            };\n")
            
            f.write("        };\n")
            f.write("    };\n\n")
            
            # Test 4: Normal condition (no fault)
            min_value = fault.properties.get('min_value', 0)
            normal_value = (min_value + max_value) // 2  # Mid-range
            
            f.write("    hil-test-normal-operation {\n")
            f.write("        compatible = \"lq,hil-test\";\n")
            f.write("        description = \"Test normal operation without faults\";\n")
            f.write("        timeout-ms = <2000>;\n")
            f.write("        \n")
            f.write("        sequence {\n")
            f.write("            step@0 {\n")
            f.write("                action = \"inject-adc\";\n")
            f.write(f"                channel = <{channel}>;\n")
            f.write(f"                value = <{normal_value}>;  /* Within normal range */\n")
            f.write("            };\n")
            
            if output_type == 'gpio':
                pin = output.properties.get('target_id', 0)
                timeout = output.properties.get('expected_response_ms', 200)
                f.write("            step@1 {\n")
                f.write("                action = \"wait-gpio-low\";\n")
                f.write(f"                pin = <{pin}>;\n")
                f.write(f"                timeout-ms = <{timeout}>;\n")
                f.write("            };\n")
            elif output_type == 'can' or output_type == 'canopen':
                timeout = output.properties.get('expected_response_ms', 1500 if output_type == 'canopen' else 200)
                if output_type == 'canopen':
                    can_id = output.properties.get('cob_id', 0x180)
                    f.write("            step@1 {\n")
                    f.write("                action = \"expect-can\";\n")
                    f.write(f"                can-id = <{can_id}>;\n")
                    f.write(f"                timeout-ms = <{timeout}>;\n")
                    f.write("            };\n")
                else:
                    pgn = output.properties.get('pgn', 61444)
                    f.write("            step@1 {\n")
                    f.write("                action = \"expect-can\";\n")
                    f.write(f"                pgn = <{pgn}>;\n")
                    f.write(f"                timeout-ms = <{timeout}>;\n")
                    f.write("            };\n")
            
            f.write("        };\n")
            f.write("    };\n\n")
        
        # Test 5: Latency test
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

def expand_eds_references(input_dts_path, output_dts_path, signals_header_path=None):
    """Find CANopen nodes with 'eds' property and expand them"""
    import os
    import sys
    
    # Add scripts directory to path for imports
    sys.path.insert(0, str(Path(__file__).parent))
    
    # Read the input DTS
    with open(input_dts_path, 'r') as f:
        dts_content = f.read()
    
    # Find canopen nodes with eds property
    # Pattern: label: canopen-device@N { ... eds = "file.eds"; ... }
    canopen_pattern = r'(\w+):\s*(canopen-device@\d+)\s*\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}'
    
    expanded_content = dts_content
    all_tpdos = []
    all_rpdos = []
    
    for match in re.finditer(canopen_pattern, dts_content):
        label = match.group(1)
        node_decl = match.group(2)
        node_content = match.group(3)
        full_node = match.group(0)
        
        # Check if this node has an 'eds' property
        eds_match = re.search(r'eds\s*=\s*"([^"]+)"', node_content)
        if not eds_match:
            continue
        
        eds_file = eds_match.group(1)
        
        # Resolve EDS path relative to DTS directory
        dts_dir = Path(input_dts_path).parent
        eds_path = dts_dir / eds_file
        
        if not eds_path.exists():
            print(f"Warning: EDS file not found: {eds_path}")
            continue
        
        # Extract node-id override if present
        node_id_match = re.search(r'node-id\s*=\s*<(\d+)>', node_content)
        node_id = int(node_id_match.group(1)) if node_id_match else None
        
        try:
            from canopen_eds_parser import parse_eds_file
            
            # Parse EDS
            eds_data = parse_eds_file(str(eds_path))
            
            # Override node-id if specified in DTS
            if node_id is not None:
                eds_data['node_id'] = node_id
                # Recalculate COB IDs
                for pdo in eds_data['tpdos']:
                    pdo_idx = eds_data['tpdos'].index(pdo)
                    pdo['cob_id'] = 0x180 + (pdo_idx * 0x100) + node_id
                for pdo in eds_data['rpdos']:
                    pdo_idx = eds_data['rpdos'].index(pdo)
                    pdo['cob_id'] = 0x200 + (pdo_idx * 0x100) + node_id
            
            # Collect signal IDs for header generation
            all_tpdos.extend(eds_data.get('tpdos', []))
            all_rpdos.extend(eds_data.get('rpdos', []))
            
            # Get address from node declaration
            address = node_decl.split('@')[1]
            
            # Generate full canopen node content
            expanded_node_content = generate_canopen_node(eds_data, label, address)
            
            # Replace the stub node with expanded version
            expanded_content = expanded_content.replace(full_node, expanded_node_content)
            
            print(f"Expanded CANopen node '{label}' from {eds_file}")
            
        except ImportError as e:
            print(f"Warning: Could not import EDS parser: {e}")
            continue
    
    # Write expanded DTS
    with open(output_dts_path, 'w') as f:
        f.write(expanded_content)
    
    # Generate signal header if requested
    if signals_header_path and (all_tpdos or all_rpdos):
        generate_canopen_signal_header(all_tpdos, all_rpdos, signals_header_path)
    
    return expanded_content


def generate_canopen_node(eds_data, label, address):
    """Generate complete CANopen DTS node content from EDS data"""
    lines = []
    
    lines.append(f"    {label}: canopen-device@{address} {{")
    lines.append(f'        compatible = "lq,protocol-canopen";')
    lines.append(f'        node-id = <{eds_data["node_id"]}>;')
    lines.append(f'        label = "{eds_data["device_name"]}";')
    lines.append(f'')
    lines.append(f'        /* Auto-generated from EDS file */')
    
    # Add TPDOs
    for idx, tpdo in enumerate(eds_data.get('tpdos', [])):
        lines.append(f'')
        lines.append(f'        tpdo{idx + 1}: tpdo@{idx} {{')
        lines.append(f'            cob-id = <{tpdo["cob_id"]}>;')
        
        for map_idx, mapping in enumerate(tpdo['mappings']):
            lines.append(f'')
            lines.append(f'            mapping@{map_idx} {{')
            lines.append(f'                index = <{mapping["index"]}>;')
            lines.append(f'                subindex = <{mapping["subindex"]}>;')
            lines.append(f'                length = <{mapping["length"]}>;')
            lines.append(f'                signal-id = <{mapping["signal_id"]}>;  /* {mapping["name"]} */')
            lines.append(f'            }};')
        
        lines.append(f'        }};')
    
    # Add RPDOs
    for idx, rpdo in enumerate(eds_data.get('rpdos', [])):
        lines.append(f'')
        lines.append(f'        rpdo{idx + 1}: rpdo@{idx} {{')
        lines.append(f'            cob-id = <{rpdo["cob_id"]}>;')
        
        for map_idx, mapping in enumerate(rpdo['mappings']):
            lines.append(f'')
            lines.append(f'            mapping@{map_idx} {{')
            lines.append(f'                index = <{mapping["index"]}>;')
            lines.append(f'                subindex = <{mapping["subindex"]}>;')
            lines.append(f'                length = <{mapping["length"]}>;')
            lines.append(f'                signal-id = <{mapping["signal_id"]}>;  /* {mapping["name"]} */')
            lines.append(f'            }};')
        
        lines.append(f'        }};')
    
    lines.append(f'    }};')
    
    return '\n'.join(lines)


def generate_canopen_signal_header(tpdos, rpdos, output_path):
    """Generate signal ID header from TPDO/RPDO data"""
    lines = []
    lines.append("/* Auto-generated CANopen signal IDs - DO NOT EDIT */")
    lines.append("")
    lines.append("#ifndef MOTOR_SIGNALS_H")
    lines.append("#define MOTOR_SIGNALS_H")
    lines.append("")
    
    # RPDO signals (commands from master)
    if rpdos:
        lines.append("/* RPDO Signals (Commands from master) */")
        for pdo_idx, rpdo in enumerate(rpdos):
            for mapping in rpdo['mappings']:
                name = mapping['name'].upper().replace(' ', '_').replace('-', '_')
                name = ''.join(c if c.isalnum() or c == '_' else '' for c in name)
                signal_id = mapping['signal_id']
                comment = f"RPDO{pdo_idx + 1}: {mapping['name']}"
                lines.append(f"#define SIG_{name:40s} {signal_id:3d}  /* {comment} */")
        lines.append("")
    
    # TPDO signals (status to master)
    if tpdos:
        lines.append("/* TPDO Signals (Status to master) */")
        for pdo_idx, tpdo in enumerate(tpdos):
            for mapping in tpdo['mappings']:
                name = mapping['name'].upper().replace(' ', '_').replace('-', '_')
                name = ''.join(c if c.isalnum() or c == '_' else '' for c in name)
                signal_id = mapping['signal_id']
                comment = f"TPDO{pdo_idx + 1}: {mapping['name']}"
                lines.append(f"#define SIG_{name:40s} {signal_id:3d}  /* {comment} */")
        lines.append("")
    
    lines.append("#endif /* MOTOR_SIGNALS_H */")
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Generate C code from devicetree')
    parser.add_argument('input_dts', help='Input devicetree file')
    parser.add_argument('output_dir', help='Output directory')
    parser.add_argument('--platform', help='Platform (stm32, samd, esp32, nrf52, zephyr, freertos, baremetal)')
    parser.add_argument('--expand-eds', action='store_true', help='Expand EDS references in DTS')
    parser.add_argument('--signals-header', help='Output path for signal ID header file')
    
    args = parser.parse_args()
    platform = args.platform
    
    input_dts = Path(args.input_dts)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    if not input_dts.exists():
        print(f"Error: Input file {input_dts} not found")
        sys.exit(1)
    
    # If expanding EDS references, do that first
    if args.expand_eds:
        expanded_dts = output_dir / 'expanded.dts'
        expand_eds_references(input_dts, expanded_dts, args.signals_header)
        print(f"Generated {expanded_dts}")
        if args.signals_header:
            print(f"Generated {args.signals_header}")
        return
    
    # Parse DTS
    with open(input_dts, 'r') as f:
        dts_content = f.read()
    
    nodes = simple_dts_parser(dts_content)
    
    # Resolve phandle references and auto-assign signal IDs
    nodes = resolve_phandles_and_assign_ids(nodes)
    
    # Calculate resource counts from devicetree
    resource_counts = calculate_resource_counts(nodes)
    
    # Generate configuration header with exact counts
    generate_config_header(resource_counts, output_dir / 'lq_config.h')
    print(f"Generated {output_dir}/lq_config.h")
    print(f"  Signals: {resource_counts['num_signals']}, "
          f"HW Inputs: {resource_counts['num_hw_inputs']}, "
          f"Merges: {resource_counts['num_merges']}, "
          f"Cyclic Outputs: {resource_counts['num_cyclic_outputs']}")
    
    # Generate files
    generate_header(nodes, output_dir / 'lq_generated.h')
    generate_source(nodes, output_dir / 'lq_generated.c')
    
    print(f"Generated {output_dir}/lq_generated.h")
    print(f"Generated {output_dir}/lq_generated.c")
    print(f"Found {len(nodes)} DTS nodes")
    
    # Auto-generate HIL tests
    generate_hil_tests(nodes, output_dir / 'lq_generated_test.dts')
    print(f"Generated {output_dir}/lq_generated_test.dts (HIL tests)")
    
    # Generate platform-specific main.c
    generate_main(nodes, output_dir / 'main.c', platform or 'baremetal')
    print(f"Generated {output_dir}/main.c (platform: {platform or 'baremetal'})")
    
    # Generate platform-specific hardware interface if requested
    if platform:
        generate_platform_hw(nodes, output_dir / 'lq_platform_hw.c', platform)
    else:
        print(f"\nTip: Add --platform=<name> to generate platform-specific ISRs")
        print(f"     Supported platforms: stm32, samd, esp32, nrf52, baremetal")

if __name__ == '__main__':
    main()
