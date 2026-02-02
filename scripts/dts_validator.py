"""
Build-time validation for devicetree configuration.

Ensures consistent and valid DTS configurations before code generation.
"""

import sys
from typing import List
from dts_parser import DTSNode


def validate_uds_dependencies(nodes: List[DTSNode]) -> None:
    """
    Validate UDS protocol dependencies.
    
    If any driver adds a UDS exposure property (expose_*_read/write),
    the user must also have a UDS protocol node defined.
    
    Raises:
        SystemExit: If validation fails
    """
    # Find all UDS exposure properties
    exposures = []
    for node in nodes:
        for prop_name in node.properties.keys():
            if prop_name.startswith('expose_'):
                exposures.append((node.label, prop_name))
    
    if not exposures:
        return  # No exposures, nothing to validate
    
    # Check for UDS protocol node
    uds_nodes = [n for n in nodes if n.compatible == 'lq,protocol-uds']
    
    if not uds_nodes:
        print("ERROR: UDS exposures found but no UDS stack defined!", file=sys.stderr)
        print(f"\nFound exposures:", file=sys.stderr)
        for label, prop in exposures:
            print(f"  - {label}: {prop}", file=sys.stderr)
        print(f"\nRequired: Add a UDS node to devicetree:", file=sys.stderr)
        print(f"  uds_can: uds-can@0 {{", file=sys.stderr)
        print(f"      compatible = \"lq,protocol-uds\";", file=sys.stderr)
        print(f"      can-device = <&can1>;", file=sys.stderr)
        print(f"  }};", file=sys.stderr)
        sys.exit(1)
    
    # Validate UDS node has required properties
    for uds_node in uds_nodes:
        if 'can_device' not in uds_node.properties:
            print(f"ERROR: UDS node '{uds_node.label}' missing can-device property!", file=sys.stderr)
            sys.exit(1)


def validate_all(nodes: List[DTSNode]) -> None:
    """
    Run all validation checks.
    
    Args:
        nodes: List of DTSNode objects
        
    Raises:
        SystemExit: If any validation fails
    """
    validate_uds_dependencies(nodes)
    # Add more validators here as needed
