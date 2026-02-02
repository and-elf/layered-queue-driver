"""Tests for config generator."""

import pytest
from generators.config import ConfigGenerator


class TestConfigGenerator:
    """Tests for ConfigGenerator class."""
    
    def test_generate_requires_counts(self):
        """Test that generate() requires counts parameter."""
        gen = ConfigGenerator()
        
        with pytest.raises(ValueError, match="requires counts dict"):
            gen.generate([], counts=None)
    
    def test_generate_returns_dict(self):
        """Test that generate() returns dict with filename."""
        gen = ConfigGenerator()
        counts = {
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
        
        result = gen.generate([], counts)
        
        assert isinstance(result, dict)
        assert 'lq_config.h' in result
        assert isinstance(result['lq_config.h'], str)
    
    def test_generated_header_has_guards(self):
        """Test that generated header has include guards."""
        gen = ConfigGenerator()
        counts = {'num_signals': 10, 'num_hw_inputs': 3, 'num_scales': 0,
                  'num_remaps': 0, 'num_merges': 0, 'num_fault_monitors': 0,
                  'num_cyclic_outputs': 0, 'num_pid_controllers': 0,
                  'num_verified_outputs': 0, 'num_health_devices': 3,
                  'max_merge_inputs': 0, 'max_output_events': 16,
                  'hw_ringbuffer_size': 128}
        
        result = gen.generate([], counts)
        content = result['lq_config.h']
        
        assert '#ifndef LQ_CONFIG_H_' in content
        assert '#define LQ_CONFIG_H_' in content
        assert '#endif /* LQ_CONFIG_H_ */' in content
    
    def test_generated_header_has_all_defines(self):
        """Test that generated header has all required defines."""
        gen = ConfigGenerator()
        counts = {
            'num_signals': 10,
            'num_hw_inputs': 3,
            'num_scales': 1,
            'num_remaps': 2,
            'num_merges': 1,
            'num_fault_monitors': 2,
            'num_cyclic_outputs': 2,
            'num_pid_controllers': 1,
            'num_verified_outputs': 1,
            'num_health_devices': 6,
            'max_merge_inputs': 3,
            'max_output_events': 16,
            'hw_ringbuffer_size': 128,
        }
        
        result = gen.generate([], counts)
        content = result['lq_config.h']
        
        assert '#define LQ_MAX_SIGNALS              10' in content
        assert '#define LQ_MAX_HW_INPUTS            3' in content
        assert '#define LQ_MAX_SCALES               1' in content
        assert '#define LQ_MAX_REMAPS               2' in content
        assert '#define LQ_MAX_MERGES               1' in content
        assert '#define LQ_MAX_FAULT_MONITORS       2' in content
        assert '#define LQ_MAX_CYCLIC_OUTPUTS       2' in content
        assert '#define LQ_MAX_PID_CONTROLLERS      1' in content
        assert '#define LQ_MAX_VERIFIED_OUTPUTS     1' in content
        assert '#define LQ_MAX_HEALTH_DEVICES       6' in content
        assert '#define LQ_MAX_MERGE_INPUTS         3' in content
        assert '#define LQ_MAX_OUTPUT_EVENTS        16' in content
        assert '#define LQ_HW_RINGBUFFER_SIZE       128' in content
    
    def test_generated_header_has_auto_generated_comment(self):
        """Test that generated header has auto-generated warning."""
        gen = ConfigGenerator()
        counts = {'num_signals': 10, 'num_hw_inputs': 0, 'num_scales': 0,
                  'num_remaps': 0, 'num_merges': 0, 'num_fault_monitors': 0,
                  'num_cyclic_outputs': 0, 'num_pid_controllers': 0,
                  'num_verified_outputs': 0, 'num_health_devices': 0,
                  'max_merge_inputs': 0, 'max_output_events': 16,
                  'hw_ringbuffer_size': 128}
        
        result = gen.generate([], counts)
        content = result['lq_config.h']
        
        assert 'AUTO-GENERATED FILE - DO NOT EDIT' in content
        assert 'Generated from devicetree' in content
    
    def test_memory_savings_calculation(self):
        """Test that memory savings are calculated."""
        gen = ConfigGenerator()
        counts = {'num_signals': 6, 'num_hw_inputs': 0, 'num_scales': 0,
                  'num_remaps': 0, 'num_merges': 0, 'num_fault_monitors': 0,
                  'num_cyclic_outputs': 0, 'num_pid_controllers': 0,
                  'num_verified_outputs': 0, 'num_health_devices': 0,
                  'max_merge_inputs': 0, 'max_output_events': 16,
                  'hw_ringbuffer_size': 128}
        
        result = gen.generate([], counts)
        content = result['lq_config.h']
        
        # 6 signals vs default 32 = ~81% savings
        assert '81%' in content or 'Savings' in content
