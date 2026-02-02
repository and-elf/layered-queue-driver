"""Tests for UDS generator."""

import pytest
from dts_parser import DTSNode
from generators.uds import UDSGenerator


class TestUDSGenerator:
    """Tests for UDSGenerator class."""
    
    def test_no_uds_returns_empty_dict(self):
        """Test that no UDS protocol returns empty dict."""
        gen = UDSGenerator()
        nodes = [
            DTSNode('adc1', 'lq,hw-adc-input', '0'),
        ]
        
        result = gen.generate(nodes)
        assert result == {}
    
    def test_uds_node_generates_files(self):
        """Test that UDS node generates header and source."""
        gen = UDSGenerator()
        
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'can_device': 'can1',
            'expose_throttle_scale_read': 0xF200,
        }
        
        throttle = DTSNode('throttle_scale', 'lq,scale', '0')
        nodes = [uds_node, throttle]
        
        result = gen.generate(nodes)
        
        assert 'lq_generated_uds.h' in result
        assert 'lq_generated_uds.c' in result
    
    def test_uds_header_has_did_defines(self):
        """Test that UDS header has DID defines."""
        gen = UDSGenerator()
        
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'can_device': 'can1',
            'expose_throttle_scale_read': 0xF200,
            'expose_throttle_scale_write': 0xF201,
        }
        
        throttle = DTSNode('throttle_scale', 'lq,scale', '0')
        nodes = [uds_node, throttle]
        
        result = gen.generate(nodes)
        content = result['lq_generated_uds.h']
        
        assert '#define UDS_DID_THROTTLE_SCALE_READ  0xF200' in content
        assert '#define UDS_DID_THROTTLE_SCALE_WRITE 0xF201' in content
    
    def test_uds_header_has_handler_declaration(self):
        """Test that UDS header has handler function declaration."""
        gen = UDSGenerator()
        
        # Create a valid target node for UDS to expose
        target_node = DTSNode('throttle_adc', 'lq,hw-adc-input', '0')
        target_node.properties = {'signal_id': 0}
        
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'can_device': 'can1',
            'expose_throttle_adc_read': 0x1234,
        }
        
        result = gen.generate([target_node, uds_node])
        content = result['lq_generated_uds.h']
        
        assert 'int lq_generated_uds_handler' in content
        assert 'uint16_t did' in content
        assert 'bool is_write' in content
    
    def test_uds_source_has_switch_case(self):
        """Test that UDS source has switch-case for DIDs."""
        gen = UDSGenerator()
        
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'can_device': 'can1',
            'expose_throttle_read': 0xF200,
        }
        
        throttle = DTSNode('throttle', 'lq,scale', '0')
        nodes = [uds_node, throttle]
        
        result = gen.generate(nodes)
        content = result['lq_generated_uds.c']
        
        assert 'switch (did)' in content
        assert 'case UDS_DID_THROTTLE_READ:' in content
        assert 'default:' in content
        assert 'return -1' in content
    
    def test_multiple_exposures(self):
        """Test multiple UDS exposures."""
        gen = UDSGenerator()
        
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'can_device': 'can1',
            'expose_throttle_read': 0xF200,
            'expose_brake_read': 0xF201,
            'expose_speed_read': 0xF202,
        }
        
        nodes = [uds_node,
                 DTSNode('throttle', 'lq,scale', '0'),
                 DTSNode('brake', 'lq,scale', '1'),
                 DTSNode('speed', 'lq,scale', '2')]
        
        result = gen.generate(nodes)
        header = result['lq_generated_uds.h']
        
        assert 'UDS_DID_THROTTLE_READ' in header
        assert 'UDS_DID_BRAKE_READ' in header
        assert 'UDS_DID_SPEED_READ' in header
    
    def test_read_write_both_supported(self):
        """Test that both read and write DIDs are generated."""
        gen = UDSGenerator()
        
        uds_node = DTSNode('uds_can', 'lq,protocol-uds', '0')
        uds_node.properties = {
            'can_device': 'can1',
            'expose_throttle_read': 0xF200,
            'expose_throttle_write': 0xF300,
        }
        
        throttle = DTSNode('throttle', 'lq,scale', '0')
        nodes = [uds_node, throttle]
        
        result = gen.generate(nodes)
        source = result['lq_generated_uds.c']
        
        assert 'case UDS_DID_THROTTLE_READ:' in source
        assert 'case UDS_DID_THROTTLE_WRITE:' in source
