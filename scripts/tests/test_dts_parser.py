"""Tests for DTS parser."""

import pytest
from dts_parser import (
    DTSNode,
    parse_property_value,
    simple_dts_parser,
    resolve_phandles_and_assign_ids,
    calculate_resource_counts,
)


class TestParsePropertyValue:
    """Tests for parse_property_value function."""
    
    def test_parse_integer(self):
        """Test parsing single integer."""
        assert parse_property_value('<123>') == 123
        assert parse_property_value('<0x10>') == 16
        assert parse_property_value('<0>') == 0
    
    def test_parse_integer_array(self):
        """Test parsing integer array."""
        assert parse_property_value('<1 2 3>') == [1, 2, 3]
        assert parse_property_value('<0x10 0x20>') == [16, 32]
    
    def test_parse_string(self):
        """Test parsing string."""
        assert parse_property_value('"median"') == 'median'
        assert parse_property_value('"hello world"') == 'hello world'
    
    def test_parse_single_phandle(self):
        """Test parsing single phandle reference."""
        assert parse_property_value('<&sensor>') == 'sensor'
        assert parse_property_value('<&throttle_adc>') == 'throttle_adc'
    
    def test_parse_phandle_array(self):
        """Test parsing multiple phandle references."""
        assert parse_property_value('<&s1 &s2 &s3>') == ['s1', 's2', 's3']
    
    def test_parse_boolean(self):
        """Test parsing boolean (empty value)."""
        assert parse_property_value('') == True


class TestSimpleDTSParser:
    """Tests for simple_dts_parser function."""
    
    def test_parse_basic_node(self):
        """Test parsing a basic DTS node."""
        dts = '''
        throttle: hw-adc@0 {
            compatible = "lq,hw-adc-input";
            hw-channel = <3>;
            hw-instance = <1>;
        };
        '''
        
        nodes = simple_dts_parser(dts)
        assert len(nodes) == 1
        assert nodes[0].label == 'throttle'
        assert nodes[0].compatible == 'lq,hw-adc-input'
        assert nodes[0].properties['hw_channel'] == 3
        assert nodes[0].properties['hw_instance'] == 1
    
    def test_parse_multiple_nodes(self):
        """Test parsing multiple DTS nodes."""
        dts = '''
        adc1: hw-adc@0 {
            compatible = "lq,hw-adc-input";
        };
        
        spi1: hw-spi@0 {
            compatible = "lq,hw-spi-input";
        };
        '''
        
        nodes = simple_dts_parser(dts)
        assert len(nodes) == 2
        assert nodes[0].label == 'adc1'
        assert nodes[1].label == 'spi1'
    
    def test_parse_boolean_properties(self):
        """Test parsing boolean properties."""
        dts = '''
        monitor: fault-monitor@0 {
            compatible = "lq,fault-monitor";
            check-staleness;
            check-range;
        };
        '''
        
        nodes = simple_dts_parser(dts)
        assert nodes[0].properties['check_staleness'] == True
        assert nodes[0].properties['check_range'] == True
    
    def test_ignore_comments(self):
        """Test that comments are properly ignored."""
        dts = '''
        // This is a comment
        node1: test@0 {
            compatible = "lq,scale";
            /* Multi-line
               comment */
            scale-factor = <100>;
        };
        '''
        
        nodes = simple_dts_parser(dts)
        assert len(nodes) == 1
        assert nodes[0].properties['scale_factor'] == 100


class TestResolvePhandlesAndAssignIds:
    """Tests for resolve_phandles_and_assign_ids function."""
    
    def test_auto_assign_signal_ids(self):
        """Test auto-assignment of signal IDs."""
        node1 = DTSNode('adc1', 'lq,hw-adc-input', '0')
        node2 = DTSNode('adc2', 'lq,hw-adc-input', '1')
        node3 = DTSNode('scale1', 'lq,scale', '0')
        
        nodes = resolve_phandles_and_assign_ids([node1, node2, node3])
        
        assert nodes[0].signal_id == 0
        assert nodes[1].signal_id == 1
        assert nodes[2].signal_id == 2
    
    def test_respect_explicit_signal_ids(self):
        """Test that explicit signal IDs are respected."""
        node1 = DTSNode('adc1', 'lq,hw-adc-input', '0')
        node1.properties['signal_id'] = 5
        
        node2 = DTSNode('adc2', 'lq,hw-adc-input', '1')
        
        nodes = resolve_phandles_and_assign_ids([node1, node2])
        
        assert nodes[0].signal_id == 5
        assert nodes[1].signal_id == 6  # Auto-assigned after 5
    
    def test_resolve_single_phandle(self):
        """Test resolving single phandle reference."""
        source = DTSNode('adc1', 'lq,hw-adc-input', '0')
        scale = DTSNode('scale1', 'lq,scale', '0')
        scale.properties['source'] = 'adc1'
        
        nodes = resolve_phandles_and_assign_ids([source, scale])
        
        assert scale.properties['source_signal'] == 0  # Resolved to signal_id
    
    def test_resolve_multiple_phandles(self):
        """Test resolving multiple phandle references."""
        adc1 = DTSNode('adc1', 'lq,hw-adc-input', '0')
        adc2 = DTSNode('adc2', 'lq,hw-adc-input', '1')
        merge = DTSNode('merge1', 'lq,mid-merge', '0')
        merge.properties['sources'] = ['adc1', 'adc2']
        
        nodes = resolve_phandles_and_assign_ids([adc1, adc2, merge])
        
        assert merge.properties['input_signal_ids'] == [0, 1]


class TestCalculateResourceCounts:
    """Tests for calculate_resource_counts function."""
    
    def test_count_hw_inputs(self):
        """Test counting hardware inputs."""
        nodes = [
            DTSNode('adc1', 'lq,hw-adc-input', '0'),
            DTSNode('spi1', 'lq,hw-spi-input', '0'),
            DTSNode('can1', 'lq,hw-can-input', '0'),
        ]
        
        counts = calculate_resource_counts(nodes)
        assert counts['num_hw_inputs'] == 3
        assert counts['num_health_devices'] == 3  # HW inputs register with health
    
    def test_count_processing_nodes(self):
        """Test counting processing nodes."""
        nodes = [
            DTSNode('scale1', 'lq,scale', '0'),
            DTSNode('remap1', 'lq,remap', '0'),
            DTSNode('pid1', 'lq,pid', '0'),
        ]
        
        counts = calculate_resource_counts(nodes)
        assert counts['num_scales'] == 1
        assert counts['num_remaps'] == 1
        assert counts['num_pid_controllers'] == 1
    
    def test_count_monitoring_nodes(self):
        """Test counting monitoring nodes."""
        nodes = [
            DTSNode('monitor1', 'lq,fault-monitor', '0'),
            DTSNode('monitor2', 'lq,fault-monitor', '1'),
        ]
        
        counts = calculate_resource_counts(nodes)
        assert counts['num_fault_monitors'] == 2
        assert counts['num_health_devices'] == 2
    
    def test_count_output_nodes(self):
        """Test counting output nodes."""
        nodes = [
            DTSNode('out1', 'lq,cyclic-output', '0'),
            DTSNode('out2', 'lq,verified-output', '0'),
        ]
        
        counts = calculate_resource_counts(nodes)
        assert counts['num_cyclic_outputs'] == 1
        assert counts['num_verified_outputs'] == 1
        assert counts['num_health_devices'] == 2
    
    def test_calculate_signal_count(self):
        """Test signal count calculation."""
        node1 = DTSNode('adc1', 'lq,hw-adc-input', '0')
        node2 = DTSNode('adc2', 'lq,hw-adc-input', '1')
        
        nodes = resolve_phandles_and_assign_ids([node1, node2])
        counts = calculate_resource_counts(nodes)
        
        assert counts['num_signals'] == 2  # Signal IDs 0, 1
    
    def test_max_merge_inputs(self):
        """Test tracking max merge inputs."""
        merge1 = DTSNode('merge1', 'lq,mid-merge', '0')
        merge1.properties['input_signal_ids'] = [0, 1, 2]
        
        merge2 = DTSNode('merge2', 'lq,mid-merge', '1')
        merge2.properties['input_signal_ids'] = [3, 4]
        
        counts = calculate_resource_counts([merge1, merge2])
        assert counts['max_merge_inputs'] == 3
