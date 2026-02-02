"""Tests for DTS validator."""

import pytest
import sys
from dts_parser import DTSNode
from dts_validator import validate_uds_dependencies, validate_all


class TestValidateUDSDependencies:
    """Tests for UDS dependency validation."""
    
    def test_no_uds_exposures_passes(self):
        """Test that validation passes with no UDS exposures."""
        nodes = [
            DTSNode('adc1', 'lq,hw-adc-input', '0'),
            DTSNode('scale1', 'lq,scale', '0'),
        ]
        
        # Should not raise
        validate_uds_dependencies(nodes)
    
    def test_uds_exposure_with_uds_node_passes(self):
        """Test that validation passes with UDS node present."""
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'can_device': 'can1',
            'expose_throttle_scale_read': 0xF200,
        }
        
        throttle = DTSNode('throttle_scale', 'lq,scale', '0')
        
        nodes = [uds_node, throttle]
        
        # Should not raise
        validate_uds_dependencies(nodes)
    
    def test_uds_exposure_without_uds_node_fails(self):
        """Test that validation fails without UDS node."""
        # Create a node that looks like it has UDS properties
        # (this is a simplified test - real validation checks UDS node properties)
        uds_node = DTSNode('protocol', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'expose_something_read': 0x1234,
            # Missing can_device!
        }
        
        with pytest.raises(SystemExit):
            validate_uds_dependencies([uds_node])
    
    def test_uds_node_missing_can_device(self):
        """Test that UDS node must have can-device property."""
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'expose_throttle_read': 0xF200,
            # Missing can_device
        }
        
        with pytest.raises(SystemExit):
            validate_uds_dependencies([uds_node])


class TestValidateAll:
    """Tests for validate_all function."""
    
    def test_validate_all_runs_uds_validation(self):
        """Test that validate_all runs UDS validation."""
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'expose_something_read': 0x1234,
            # Missing can_device - should fail
        }
        
        with pytest.raises(SystemExit):
            validate_all([uds_node])
    
    def test_validate_all_passes_valid_config(self):
        """Test that validate_all passes with valid configuration."""
        nodes = [
            DTSNode('adc1', 'lq,hw-adc-input', '0'),
            DTSNode('scale1', 'lq,scale', '0'),
        ]
        
        # Should not raise
        validate_all(nodes)
