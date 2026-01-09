#!/usr/bin/env python3
"""
CANopen EDS/DCF Parser - Generate Device Tree Bindings

Parses CANopen Electronic Data Sheet (EDS) or Device Configuration File (DCF)
and generates Zephyr device tree bindings for the Layered Queue Driver.

EDS files describe the CANopen object dictionary, PDO mappings, and device
capabilities. This tool extracts that information and creates device tree
nodes that can be used in Zephyr applications.

Usage:
    python3 canopen_eds_parser.py device.eds [--node-id 5] [--output device.dts]

Features:
- Parses object dictionary entries
- Extracts TPDO/RPDO mappings
- Generates signal definitions from mapped objects
- Creates device tree protocol nodes
- Supports CiA DS-301 and device-specific profiles

Example output:
    / {
        canopen_motor: canopen-motor@5 {
            compatible = "lq,protocol-canopen";
            node-id = <5>;
            
            tpdo1: tpdo@0 {
                cob-id = <0x185>;  // 0x180 + node_id
                transmission-type = <254>;  // Event-driven
                
                mapping@0 {
                    index = <0x6041>;    // Status word
                    subindex = <0>;
                    length = <16>;
                    signal-id = <10>;    // Maps to SIG_STATUS
                };
                
                mapping@1 {
                    index = <0x606C>;    // Velocity actual
                    subindex = <0>;
                    length = <32>;
                    signal-id = <11>;    // Maps to SIG_VELOCITY
                };
            };
            
            rpdo1: rpdo@0 {
                cob-id = <0x205>;  // 0x200 + node_id
                
                mapping@0 {
                    index = <0x6040>;    // Control word
                    subindex = <0>;
                    length = <16>;
                    signal-id = <0>;     // Maps to SIG_CONTROL
                };
            };
        };
    };
"""

import configparser
import argparse
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Optional

class CANopenObject:
    """Represents a CANopen object dictionary entry"""
    def __init__(self, index: int, name: str, obj_type: int):
        self.index = index
        self.name = name
        self.object_type = obj_type  # 7=VAR, 8=ARRAY, 9=RECORD
        self.data_type = 0
        self.access_type = ""
        self.pdo_mapping = False
        self.default_value = None
        self.subindices: Dict[int, 'CANopenSubObject'] = {}
        
class CANopenSubObject:
    """Represents a subindex of a CANopen object"""
    def __init__(self, subindex: int, name: str):
        self.subindex = subindex
        self.name = name
        self.data_type = 0
        self.access_type = ""
        self.pdo_mapping = False
        self.default_value = None
        self.bit_length = 0

class PDOMapping:
    """Represents a PDO mapping entry"""
    def __init__(self, index: int, subindex: int, length: int):
        self.index = index
        self.subindex = subindex
        self.length = length  # in bits
        self.signal_id = None
        self.name = ""

class CANopenDevice:
    """Complete CANopen device description from EDS/DCF"""
    def __init__(self):
        self.vendor_name = ""
        self.product_name = ""
        self.vendor_id = 0
        self.product_code = 0
        self.revision = ""
        self.order_code = ""
        self.baudrate_10 = True
        self.baudrate_20 = True
        self.baudrate_50 = True
        self.baudrate_125 = True
        self.baudrate_250 = True
        self.baudrate_500 = True
        self.baudrate_800 = True
        self.baudrate_1000 = True
        
        self.objects: Dict[int, CANopenObject] = {}
        self.tpdo_mappings: List[List[PDOMapping]] = [[], [], [], []]  # 4 TPDOs
        self.rpdo_mappings: List[List[PDOMapping]] = [[], [], [], []]  # 4 RPDOs
        
def parse_eds(filepath: str) -> CANopenDevice:
    """Parse an EDS/DCF file"""
    device = CANopenDevice()
    config = configparser.ConfigParser()
    config.read(filepath)
    
    # Parse DeviceInfo
    if 'DeviceInfo' in config:
        dev_info = config['DeviceInfo']
        device.vendor_name = dev_info.get('VendorName', '')
        device.product_name = dev_info.get('ProductName', '')
        device.vendor_id = int(dev_info.get('VendorNumber', '0'), 0)
        device.product_code = int(dev_info.get('ProductNumber', '0'), 0)
        device.revision = dev_info.get('RevisionNumber', '0')
        device.order_code = dev_info.get('OrderCode', '')
        
    # Parse object dictionary
    for section in config.sections():
        # Object entry: [1000], [6040], [0x1A00], etc.
        section_lower = section.lower()
        
        # Skip subindex sections in first pass - they'll be handled below
        if 'sub' in section_lower:
            continue
        
        # Check if this is a hex number (with or without 0x prefix)
        try:
            # Try parsing as hex with explicit prefix handling
            if section_lower.startswith('0x'):
                index = int(section, 16)
            elif all(c in '0123456789abcdef' for c in section_lower) and len(section) <= 4:
                # Plain hex number without 0x
                index = int(section, 16)
            else:
                # Not a hex index, skip
                continue
        except ValueError:
            # Not a valid index
            continue
        
        if index is not None:
            obj_config = config[section]
            
            obj = CANopenObject(
                index=index,
                name=obj_config.get('ParameterName', f'Object_{index:04X}'),
                obj_type=int(obj_config.get('ObjectType', '7'), 0)
            )
            obj.data_type = int(obj_config.get('DataType', '0'), 0)
            obj.access_type = obj_config.get('AccessType', 'rw')
            obj.pdo_mapping = obj_config.get('PDOMapping', '0') == '1'
            obj.default_value = obj_config.get('DefaultValue')
            
            device.objects[index] = obj
    
    # Second pass - process subindex sections
    for section in config.sections():
        section_lower = section.lower()
        
        # Subindex entry: [1018sub1], [6040sub0], etc.
        if 'sub' in section_lower:
            parts = section_lower.split('sub')
            if len(parts) == 2:
                # Parse index (may or may not have 0x prefix)
                idx_str = parts[0].strip()
                if idx_str.startswith('0x'):
                    index = int(idx_str, 16)
                elif all(c in '0123456789abcdef' for c in idx_str):
                    index = int(idx_str, 16)
                else:
                    continue
                
                # Parse subindex
                sub_str = parts[1].strip()
                if sub_str.startswith('0x'):
                    subindex = int(sub_str, 16)
                elif sub_str.isdigit():
                    subindex = int(sub_str, 10)
                elif all(c in '0123456789abcdef' for c in sub_str):
                    subindex = int(sub_str, 16)
                else:
                    continue
                
                if index in device.objects:
                    sub_config = config[section]
                    sub = CANopenSubObject(
                        subindex=subindex,
                        name=sub_config.get('ParameterName', f'SubIndex_{subindex}')
                    )
                    sub.data_type = int(sub_config.get('DataType', '0'), 0)
                    sub.access_type = sub_config.get('AccessType', 'rw')
                    sub.pdo_mapping = sub_config.get('PDOMapping', '0') == '1'
                    sub.bit_length = get_data_type_length(sub.data_type)
                    sub.default_value = sub_config.get('DefaultValue')
                    
                    device.objects[index].subindices[subindex] = sub
    
    # Parse TPDO mappings (0x1A00-0x1A03)
    for pdo_idx in range(4):
        mapping_index = 0x1A00 + pdo_idx
        if mapping_index in device.objects:
            obj = device.objects[mapping_index]
            num_mapped = 0
            
            # Subindex 0 contains number of mapped objects
            if 0 in obj.subindices:
                default_val = obj.subindices[0].default_value
                if default_val:
                    num_mapped = int(default_val, 0) if isinstance(default_val, str) else int(default_val)
            
            # Subindices 1-8 contain mapped object references
            for map_idx in range(1, min(num_mapped + 1, 9)):
                if map_idx in obj.subindices:
                    default_val = obj.subindices[map_idx].default_value
                    if default_val:
                        map_value = int(default_val, 0) if isinstance(default_val, str) else int(default_val)
                        if map_value:
                            # Format: 0xIIIISSLL (Index 16bit, Subindex 8bit, Length 8bit)
                            idx = (map_value >> 16) & 0xFFFF
                            sub = (map_value >> 8) & 0xFF
                            length = map_value & 0xFF
                            
                            mapping = PDOMapping(idx, sub, length)
                            if idx in device.objects:
                                mapping.name = device.objects[idx].name
                                if sub in device.objects[idx].subindices:
                                    mapping.name = device.objects[idx].subindices[sub].name
                            
                            device.tpdo_mappings[pdo_idx].append(mapping)
    
    # Parse RPDO mappings (0x1600-0x1603)
    for pdo_idx in range(4):
        mapping_index = 0x1600 + pdo_idx
        if mapping_index in device.objects:
            obj = device.objects[mapping_index]
            num_mapped = 0
            
            if 0 in obj.subindices:
                default_val = obj.subindices[0].default_value
                if default_val:
                    num_mapped = int(default_val, 0) if isinstance(default_val, str) else int(default_val)
            
            for map_idx in range(1, min(num_mapped + 1, 9)):
                if map_idx in obj.subindices:
                    default_val = obj.subindices[map_idx].default_value
                    if default_val:
                        map_value = int(default_val, 0) if isinstance(default_val, str) else int(default_val)
                        if map_value:
                            idx = (map_value >> 16) & 0xFFFF
                            sub = (map_value >> 8) & 0xFF
                            length = map_value & 0xFF
                            
                            mapping = PDOMapping(idx, sub, length)
                            if idx in device.objects:
                                mapping.name = device.objects[idx].name
                                if sub in device.objects[idx].subindices:
                                    mapping.name = device.objects[idx].subindices[sub].name
                            
                            device.rpdo_mappings[pdo_idx].append(mapping)
    
    return device

def get_data_type_length(data_type: int) -> int:
    """Get bit length for CANopen data type"""
    type_lengths = {
        0x0001: 1,   # BOOLEAN
        0x0002: 8,   # INTEGER8
        0x0003: 16,  # INTEGER16
        0x0004: 32,  # INTEGER32
        0x0005: 8,   # UNSIGNED8
        0x0006: 16,  # UNSIGNED16
        0x0007: 32,  # UNSIGNED32
        0x0008: 32,  # REAL32
        0x0009: 0,   # VISIBLE_STRING
        0x000A: 0,   # OCTET_STRING
        0x000B: 0,   # UNICODE_STRING
        0x0010: 64,  # INTEGER64
        0x001B: 64,  # UNSIGNED64
    }
    return type_lengths.get(data_type, 0)

def generate_device_tree(device: CANopenDevice, node_id: int, 
                         node_name: str, output_file: Optional[str] = None) -> str:
    """Generate Zephyr device tree overlay from CANopen device description"""
    
    lines = []
    lines.append("/*")
    lines.append(f" * CANopen Device Tree - Generated from EDS")
    lines.append(f" * Device: {device.product_name}")
    lines.append(f" * Vendor: {device.vendor_name}")
    lines.append(f" * Node ID: {node_id}")
    lines.append(" */")
    lines.append("")
    lines.append("/ {")
    lines.append(f"    {node_name}: canopen-device@{node_id} {{")
    lines.append(f'        compatible = "lq,protocol-canopen";')
    lines.append(f"        node-id = <{node_id}>;")
    lines.append(f'        label = "{device.product_name}";')
    lines.append("")
    
    # Add device identity
    if device.vendor_id or device.product_code:
        lines.append("        /* Device Identity (Object 0x1018) */")
        if device.vendor_id:
            lines.append(f"        vendor-id = <0x{device.vendor_id:08X}>;")
        if device.product_code:
            lines.append(f"        product-code = <0x{device.product_code:08X}>;")
        lines.append("")
    
    # Generate TPDO configurations
    signal_id = 100  # Start signal IDs at 100 for TPDO signals
    for pdo_idx in range(4):
        if device.tpdo_mappings[pdo_idx]:
            lines.append(f"        /* TPDO{pdo_idx + 1} - Transmit data to master */")
            lines.append(f"        tpdo{pdo_idx + 1}: tpdo@{pdo_idx} {{")
            cob_id = 0x180 + (pdo_idx * 0x100) + node_id
            lines.append(f"            cob-id = <0x{cob_id:03X}>;")
            lines.append(f"            transmission-type = <254>;  /* Event-driven */")
            lines.append(f"            event-timer-ms = <1000>;")
            lines.append("")
            
            for map_idx, mapping in enumerate(device.tpdo_mappings[pdo_idx]):
                mapping.signal_id = signal_id
                signal_id += 1
                
                lines.append(f"            /* {mapping.name} */")
                lines.append(f"            mapping@{map_idx} {{")
                lines.append(f"                index = <0x{mapping.index:04X}>;")
                lines.append(f"                subindex = <{mapping.subindex}>;")
                lines.append(f"                length = <{mapping.length}>;")
                lines.append(f"                signal-id = <{mapping.signal_id}>;  /* SIG_{sanitize_name(mapping.name)} */")
                lines.append(f"            }};")
                lines.append("")
            
            lines.append(f"        }};")
            lines.append("")
    
    # Generate RPDO configurations
    signal_id = 0  # Start signal IDs at 0 for RPDO signals (commands from master)
    for pdo_idx in range(4):
        if device.rpdo_mappings[pdo_idx]:
            lines.append(f"        /* RPDO{pdo_idx + 1} - Receive commands from master */")
            lines.append(f"        rpdo{pdo_idx + 1}: rpdo@{pdo_idx} {{")
            cob_id = 0x200 + (pdo_idx * 0x100) + node_id
            lines.append(f"            cob-id = <0x{cob_id:03X}>;")
            lines.append("")
            
            for map_idx, mapping in enumerate(device.rpdo_mappings[pdo_idx]):
                mapping.signal_id = signal_id
                signal_id += 1
                
                lines.append(f"            /* {mapping.name} */")
                lines.append(f"            mapping@{map_idx} {{")
                lines.append(f"                index = <0x{mapping.index:04X}>;")
                lines.append(f"                subindex = <{mapping.subindex}>;")
                lines.append(f"                length = <{mapping.length}>;")
                lines.append(f"                signal-id = <{mapping.signal_id}>;  /* SIG_{sanitize_name(mapping.name)} */")
                lines.append(f"            }};")
                lines.append("")
            
            lines.append(f"        }};")
            lines.append("")
    
    lines.append("    };")
    lines.append("};")
    lines.append("")
    
    # Generate signal ID definitions header
    lines.append("/*")
    lines.append(" * Signal ID Definitions")
    lines.append(" * Copy these to your application header file")
    lines.append(" */")
    lines.append("")
    lines.append("/* RPDO Signals (Commands from master) */")
    for pdo_idx in range(4):
        for mapping in device.rpdo_mappings[pdo_idx]:
            sig_name = f"SIG_{sanitize_name(mapping.name)}"
            lines.append(f"#define {sig_name:40} {mapping.signal_id:3}  /* RPDO{pdo_idx+1}: {mapping.name} */")
    
    lines.append("")
    lines.append("/* TPDO Signals (Status to master) */")
    for pdo_idx in range(4):
        for mapping in device.tpdo_mappings[pdo_idx]:
            sig_name = f"SIG_{sanitize_name(mapping.name)}"
            lines.append(f"#define {sig_name:40} {mapping.signal_id:3}  /* TPDO{pdo_idx+1}: {mapping.name} */")
    
    dts_content = '\n'.join(lines)
    
    if output_file:
        with open(output_file, 'w') as f:
            f.write(dts_content)
        print(f"Generated device tree: {output_file}")
    
    return dts_content

def generate_signal_header(device, output_file):
    """Generate C header file with signal ID definitions"""
    lines = []
    lines.append("/* Auto-generated CANopen signal IDs - DO NOT EDIT */")
    lines.append(f"/* Generated from: {device.product_name} */")
    lines.append("")
    lines.append("#ifndef MOTOR_SIGNALS_H")
    lines.append("#define MOTOR_SIGNALS_H")
    lines.append("")
    
    # RPDO signals (commands from master)
    if device.rpdo_mappings:
        lines.append("/* RPDO Signals (Commands from master) */")
        for pdo_num, mappings in enumerate(device.rpdo_mappings):
            if mappings:
                for i, mapping in enumerate(mappings):
                    obj = device.objects.get(mapping.index)
                    if obj:
                        name = sanitize_name(obj.name)
                        signal_id = pdo_num * 32 + i
                        comment = f"RPDO{pdo_num + 1}: {obj.name}"
                        lines.append(f"#define SIG_{name:40s} {signal_id:3d}  /* {comment} */")
        lines.append("")
    
    # TPDO signals (status to master)
    if device.tpdo_mappings:
        lines.append("/* TPDO Signals (Status to master) */")
        for pdo_num, mappings in enumerate(device.tpdo_mappings):
            if mappings:
                for i, mapping in enumerate(mappings):
                    obj = device.objects.get(mapping.index)
                    if obj:
                        name = sanitize_name(obj.name)
                        signal_id = 100 + pdo_num * 32 + i
                        comment = f"TPDO{pdo_num + 1}: {obj.name}"
                        lines.append(f"#define SIG_{name:40s} {signal_id:3d}  /* {comment} */")
        lines.append("")
    
    lines.append("#endif /* MOTOR_SIGNALS_H */")
    
    with open(output_file, 'w') as f:
        f.write('\n'.join(lines))

def sanitize_name(name: str) -> str:
    """Convert object name to valid C identifier"""
    # Remove special characters, replace spaces with underscores
    name = name.upper()
    name = name.replace(' ', '_')
    name = name.replace('-', '_')
    name = ''.join(c if c.isalnum() or c == '_' else '' for c in name)
    return name

def parse_eds_file(eds_path):
    """Helper function to parse EDS and return data for code generation
    
    Returns a dict with:
    - device_name: str
    - node_id: int
    - vendor_id: int
    - product_code: int
    - tpdos: list of dicts with cob_id and mappings
    - rpdos: list of dicts with cob_id and mappings
    """
    device = parse_eds(eds_path)
    
    result = {
        'device_name': device.product_name,
        'node_id': 5,  # Default node ID
        'vendor_id': device.vendor_id,
        'product_code': device.product_code,
        'tpdos': [],
        'rpdos': []
    }
    
    # Convert TPDOs
    for pdo_idx, mappings in enumerate(device.tpdo_mappings):
        if mappings:
            tpdo = {
                'cob_id': 0x180 + (pdo_idx * 0x100) + result['node_id'],
                'mappings': []
            }
            for map_idx, mapping in enumerate(mappings):
                obj = device.objects.get(mapping.index)
                name = obj.name if obj else f"TPDO{pdo_idx + 1}_Signal{map_idx}"
                
                tpdo['mappings'].append({
                    'index': mapping.index,
                    'subindex': mapping.subindex,
                    'length': mapping.length,
                    'signal_id': 100 + pdo_idx * 32 + map_idx,
                    'name': name
                })
            result['tpdos'].append(tpdo)
    
    # Convert RPDOs
    for pdo_idx, mappings in enumerate(device.rpdo_mappings):
        if mappings:
            rpdo = {
                'cob_id': 0x200 + (pdo_idx * 0x100) + result['node_id'],
                'mappings': []
            }
            for map_idx, mapping in enumerate(mappings):
                obj = device.objects.get(mapping.index)
                name = obj.name if obj else f"RPDO{pdo_idx + 1}_Signal{map_idx}"
                
                rpdo['mappings'].append({
                    'index': mapping.index,
                    'subindex': mapping.subindex,
                    'length': mapping.length,
                    'signal_id': pdo_idx * 32 + map_idx,
                    'name': name
                })
            result['rpdos'].append(rpdo)
    
    return result

def main():
    parser = argparse.ArgumentParser(
        description='Parse CANopen EDS/DCF and generate device tree overlay',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('eds_file', help='Path to EDS or DCF file')
    parser.add_argument('--node-id', type=int, default=1, 
                       help='CANopen node ID (1-127, default: 1)')
    parser.add_argument('--node-name', default='canopen_device',
                       help='Device tree node name (default: canopen_device)')
    parser.add_argument('--output', '-o', help='Output DTS file (default: stdout)')
    parser.add_argument('--signals-header', help='Output C header file with signal ID definitions')
    parser.add_argument('--list-objects', action='store_true',
                       help='List all object dictionary entries')
    
    args = parser.parse_args()
    
    # Parse EDS file
    try:
        device = parse_eds(args.eds_file)
    except Exception as e:
        import traceback
        print(f"Error parsing EDS file: {e}", file=sys.stderr)
        traceback.print_exc()
        return 1
    
    # List objects if requested
    if args.list_objects:
        print(f"\n{'Index':6} {'Name':40} {'Type':8} {'PDO':3}")
        print("-" * 60)
        for index, obj in sorted(device.objects.items()):
            pdo_flag = 'Y' if obj.pdo_mapping else 'N'
            print(f"0x{index:04X}  {obj.name:40} {obj.object_type:8} {pdo_flag:3}")
            for subidx, sub in sorted(obj.subindices.items()):
                sub_pdo = 'Y' if sub.pdo_mapping else 'N'
                print(f"  [{subidx}]  {sub.name:38} {sub.data_type:8} {sub_pdo:3}")
        print()
    
    # Generate device tree
    dts_content = generate_device_tree(device, args.node_id, args.node_name, args.output)
    
    if not args.output:
        print(dts_content)
    
    # Generate signal header if requested
    if args.signals_header:
        generate_signal_header(device, args.signals_header)
        print(f"Generated signal header: {args.signals_header}", file=sys.stderr)
    
    # Print summary
    print(f"\nDevice: {device.product_name}", file=sys.stderr)
    print(f"Vendor: {device.vendor_name}", file=sys.stderr)
    print(f"Node ID: {args.node_id}", file=sys.stderr)
    
    tpdo_count = sum(1 for mappings in device.tpdo_mappings if mappings)
    rpdo_count = sum(1 for mappings in device.rpdo_mappings if mappings)
    print(f"TPDOs configured: {tpdo_count}", file=sys.stderr)
    print(f"RPDOs configured: {rpdo_count}", file=sys.stderr)
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
