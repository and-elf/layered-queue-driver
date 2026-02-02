"""Pytest configuration and fixtures."""

import sys
from pathlib import Path
import pytest

# Add scripts directory to path
scripts_dir = Path(__file__).parent.parent
sys.path.insert(0, str(scripts_dir))

from dts_parser import DTSNode


@pytest.fixture
def sample_adc_node():
    """Sample ADC input node."""
    node = DTSNode('throttle_adc', 'lq,hw-adc-input', '0')
    node.properties = {
        'signal_id': 0,
        'hw_instance': 1,
        'hw_channel': 3,
    }
    node.signal_id = 0
    return node


@pytest.fixture
def sample_spi_node():
    """Sample SPI input node."""
    node = DTSNode('encoder_spi', 'lq,hw-spi-input', '0')
    node.properties = {
        'signal_id': 1,
        'hw_instance': 1,
    }
    node.signal_id = 1
    return node


@pytest.fixture
def sample_can_node():
    """Sample CAN input node."""
    node = DTSNode('engine_temp', 'lq,hw-can-input', '0')
    node.properties = {
        'signal_id': 2,
        'hw_instance': 1,
        'pgn': 0xF004,
    }
    node.signal_id = 2
    return node


@pytest.fixture
def sample_scale_node():
    """Sample scale processing node."""
    node = DTSNode('throttle_scale', 'lq,scale', '0')
    node.properties = {
        'signal_id': 3,
        'source_signal': 0,
        'scale_factor': 100,
        'offset': 0,
    }
    node.signal_id = 3
    return node


@pytest.fixture
def sample_fault_monitor_node():
    """Sample fault monitor node."""
    node = DTSNode('throttle_monitor', 'lq,fault-monitor', '0')
    node.properties = {
        'input_signal': 0,
        'fault_output_signal_id': 10,
        'check_staleness': True,
        'stale_timeout_us': 50000,
        'check_range': True,
        'min_value': 0,
        'max_value': 4095,
    }
    return node


@pytest.fixture
def sample_nodes(sample_adc_node, sample_spi_node, sample_scale_node):
    """Collection of sample nodes."""
    return [sample_adc_node, sample_spi_node, sample_scale_node]


@pytest.fixture
def sample_resource_counts():
    """Sample resource counts."""
    return {
        'num_signals': 10,
        'num_hw_inputs': 3,
        'num_scales': 1,
        'num_remaps': 0,
        'num_merges': 1,
        'num_fault_monitors': 2,
        'num_cyclic_outputs': 2,
        'num_pid_controllers': 0,
        'num_verified_outputs': 1,
        'num_health_devices': 6,
        'max_merge_inputs': 3,
        'max_output_events': 16,
        'hw_ringbuffer_size': 128,
    }
