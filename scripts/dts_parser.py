"""
Devicetree parser for layered-queue-driver.

Extracts nodes and properties from DTS files and resolves phandle references.
"""

import re
from typing import List, Dict, Any


class DTSNode:
    """Represents a devicetree node with properties."""
    
    def __init__(self, label: str, compatible: str, address: str = None):
        self.label = label
        self.compatible = compatible
        self.address = address
        self.properties: Dict[str, Any] = {}
        self.children: List['DTSNode'] = []
        self.signal_id: int = None  # Auto-assigned during resolution


def parse_property_value(value: str) -> Any:
    """
    Parse DTS property value - handle <>, "", arrays, phandles.
    
    Examples:
        <&sensor> -> "sensor" (phandle)
        <&s1 &s2> -> ["s1", "s2"] (phandle array)
        <123> -> 123 (integer)
        <1 2 3> -> [1, 2, 3] (integer array)
        "median" -> "median" (string)
    """
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


def simple_dts_parser(dts_content: str) -> List[DTSNode]:
    """
    Simplified DTS parser - extracts compatible nodes with properties.
    
    Args:
        dts_content: Raw devicetree source content
        
    Returns:
        List of DTSNode objects
    """
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


def resolve_phandles_and_assign_ids(nodes: List[DTSNode]) -> List[DTSNode]:
    """
    Resolve phandle references to signal IDs and auto-assign IDs.
    
    Unified property names:
    - source: single input (scale, remap, PID)
    - sources: multiple inputs (merge/voter)
    - input: monitoring input (fault-monitor)
    - output: explicit output signal (optional, auto-assigned if not specified)
    
    Backward compatibility:
    - signal-id, source-signal, input-signal, etc. still work
    
    Args:
        nodes: List of DTSNode objects
        
    Returns:
        Updated nodes with resolved phandles and assigned signal IDs
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


def calculate_resource_counts(nodes: List[DTSNode]) -> Dict[str, int]:
    """
    Analyze devicetree nodes and calculate exact resource requirements.
    
    Args:
        nodes: List of DTSNode objects
        
    Returns:
        Dict with resource counts (signals, drivers, buffers, etc.)
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
        'num_health_devices': 0,
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
            counts['num_health_devices'] += 1  # All hardware inputs register with health
        elif node.compatible in ['lq-scale', 'lq,scale']:
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
            counts['num_health_devices'] += 1  # Monitors can register health status
        elif node.compatible == 'lq,cyclic-output':
            counts['num_cyclic_outputs'] += 1
            counts['num_health_devices'] += 1  # Outputs register with health
        elif node.compatible in ['lq,pid', 'lq,pid-controller']:
            counts['num_pid_controllers'] += 1
        elif node.compatible == 'lq,verified-output':
            counts['num_verified_outputs'] += 1
            counts['num_health_devices'] += 1  # Critical outputs register with health
        elif node.compatible == 'lq,gpio-pattern':
            counts['num_health_devices'] += 1  # GPIO patterns register with health
    
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
