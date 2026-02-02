"""Integration tests for the complete generator pipeline."""

import pytest
from pathlib import Path
import tempfile
from dts_parser import simple_dts_parser, resolve_phandles_and_assign_ids, calculate_resource_counts
from dts_validator import validate_all
from generators import ConfigGenerator, CoreGenerator, UDSGenerator, HILGenerator, PlatformGenerator


class TestGeneratorPipeline:
    """Test the complete code generation pipeline."""
    
    def test_simple_dts_complete_pipeline(self):
        """Test complete pipeline with simple DTS."""
        dts_content = '''
        engine: lq-engine@0 {
            compatible = "lq,engine";
        };
        
        throttle: hw-adc@0 {
            compatible = "lq,hw-adc-input";
            hw-channel = <3>;
            hw-instance = <1>;
        };
        
        throttle_scale: scale@0 {
            compatible = "lq,scale";
            source = <&throttle>;
            scale-factor = <100>;
        };
        '''
        
        # Parse
        nodes = simple_dts_parser(dts_content)
        assert len(nodes) == 3
        
        # Resolve
        nodes = resolve_phandles_and_assign_ids(nodes)
        assert nodes[1].signal_id == 0  # throttle
        assert nodes[2].signal_id == 1  # throttle_scale
        
        # Validate
        validate_all(nodes)  # Should not raise
        
        # Calculate counts
        counts = calculate_resource_counts(nodes)
        assert counts['num_signals'] == 2
        assert counts['num_hw_inputs'] == 1
        assert counts['num_scales'] == 1
        
        # Generate config
        config_gen = ConfigGenerator()
        config_output = config_gen.generate(nodes, counts)
        assert 'lq_config.h' in config_output
        assert '#define LQ_MAX_SIGNALS              2' in config_output['lq_config.h']
        
        # Generate core
        core_gen = CoreGenerator()
        core_output = core_gen.generate(nodes, counts)
        assert 'lq_generated.h' in core_output
        assert 'lq_generated.c' in core_output
        
        # Generate platform code
        platform_gen = PlatformGenerator('stm32')
        platform_output = platform_gen.generate(nodes, counts)
        assert 'lq_platform_hw.c' in platform_output
        assert 'main.c' in platform_output
        assert 'HAL_ADC_ConvCpltCallback' in platform_output['lq_platform_hw.c']
    
    def test_uds_integration(self):
        """Test UDS generation in complete pipeline."""
        dts_content = '''
        throttle: hw-adc@0 {
            compatible = "lq,hw-adc-input";
        };
        
        uds_stack: uds-can@0 {
            compatible = "lq,protocol-uds";
            can-device = <&can1>;
            expose-throttle-read = <0xF200>;
        };
        '''
        
        nodes = simple_dts_parser(dts_content)
        nodes = resolve_phandles_and_assign_ids(nodes)
        validate_all(nodes)
        counts = calculate_resource_counts(nodes)
        
        # Generate UDS
        uds_gen = UDSGenerator()
        uds_output = uds_gen.generate(nodes, counts)
        
        assert 'lq_generated_uds.h' in uds_output
        assert 'lq_generated_uds.c' in uds_output
        assert 'UDS_DID_THROTTLE_READ' in uds_output['lq_generated_uds.h']
    
    def test_all_generators_io_free(self):
        """Test that all generators return dicts without doing I/O."""
        dts_content = '''
        adc1: hw-adc@0 {
            compatible = "lq,hw-adc-input";
        };
        '''
        
        nodes = simple_dts_parser(dts_content)
        nodes = resolve_phandles_and_assign_ids(nodes)
        counts = calculate_resource_counts(nodes)
        
        # All generators should return dicts
        generators = [
            ConfigGenerator(),
            CoreGenerator(),
            UDSGenerator(),
            HILGenerator(),
            PlatformGenerator('baremetal'),
        ]
        
        for gen in generators:
            result = gen.generate(nodes, counts)
            assert isinstance(result, dict)
            
            # All values should be strings (file contents)
            for filename, content in result.items():
                assert isinstance(filename, str)
                assert isinstance(content, str)
    
    def test_orchestrator_pattern(self):
        """Test orchestrator pattern (collect all outputs)."""
        dts_content = '''
        throttle: hw-adc@0 {
            compatible = "lq,hw-adc-input";
        };
        '''
        
        nodes = simple_dts_parser(dts_content)
        nodes = resolve_phandles_and_assign_ids(nodes)
        counts = calculate_resource_counts(nodes)
        
        # Orchestrate all generators
        outputs = {}
        outputs.update(ConfigGenerator().generate(nodes, counts))
        outputs.update(CoreGenerator().generate(nodes, counts))
        outputs.update(UDSGenerator().generate(nodes, counts))
        outputs.update(HILGenerator().generate(nodes, counts))
        outputs.update(PlatformGenerator('stm32').generate(nodes, counts))
        
        # Should have all generated files
        expected_files = [
            'lq_config.h',
            'lq_generated.h',
            'lq_generated.c',
            'lq_generated_test.dts',
            'lq_platform_hw.c',
            'main.c',
        ]
        
        for filename in expected_files:
            assert filename in outputs, f"Missing {filename}"
            assert len(outputs[filename]) > 0, f"{filename} is empty"
    
    def test_atomic_write_simulation(self, tmp_path):
        """Test atomic write pattern (all-or-nothing)."""
        dts_content = '''
        adc1: hw-adc@0 {
            compatible = "lq,hw-adc-input";
        };
        '''
        
        nodes = simple_dts_parser(dts_content)
        nodes = resolve_phandles_and_assign_ids(nodes)
        counts = calculate_resource_counts(nodes)
        
        # Collect all outputs
        outputs = {}
        outputs.update(ConfigGenerator().generate(nodes, counts))
        outputs.update(CoreGenerator().generate(nodes, counts))
        
        # Simulate atomic write
        output_dir = tmp_path / "generated"
        output_dir.mkdir()
        
        # Write all files (simulating orchestrator)
        for filename, content in outputs.items():
            (output_dir / filename).write_text(content)
        
        # Verify all files exist
        assert (output_dir / 'lq_config.h').exists()
        assert (output_dir / 'lq_generated.h').exists()
        assert (output_dir / 'lq_generated.c').exists()
        
        # Verify content
        config_content = (output_dir / 'lq_config.h').read_text()
        assert 'LQ_MAX_SIGNALS' in config_content
