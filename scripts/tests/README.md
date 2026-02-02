# Generator Tests

Comprehensive pytest test suite for the refactored DTS code generators.

## Running Tests

```bash
# Run all tests
cd scripts
pytest tests/

# Run with verbose output
pytest tests/ -v

# Run with coverage
pytest tests/ --cov=. --cov-report=html

# Run specific test file
pytest tests/test_dts_parser.py

# Run specific test class
pytest tests/test_platform_generators.py::TestSTM32Generator

# Run specific test
pytest tests/test_config_generator.py::TestConfigGenerator::test_generate_returns_dict
```

## Test Structure

```
tests/
├── __init__.py                    # Test package marker
├── conftest.py                    # Pytest fixtures and configuration
├── test_dts_parser.py            # DTS parsing tests
├── test_dts_validator.py         # Validation tests
├── test_config_generator.py      # Config generator tests
├── test_platform_generators.py   # Platform generator tests
├── test_uds_generator.py         # UDS generator tests
└── test_integration.py           # End-to-end integration tests
```

## Test Coverage

### DTS Parser (`test_dts_parser.py`)
- ✅ Property value parsing (integers, arrays, strings, phandles)
- ✅ Basic DTS parsing (nodes, properties, boolean flags)
- ✅ Comment handling
- ✅ Phandle resolution
- ✅ Auto-assignment of signal IDs
- ✅ Resource count calculation

### DTS Validator (`test_dts_validator.py`)
- ✅ UDS dependency validation
- ✅ Build-time error detection
- ✅ Error message verification

### Config Generator (`test_config_generator.py`)
- ✅ Header generation with all defines
- ✅ Include guards
- ✅ Memory savings calculation
- ✅ Auto-generated comment warnings
- ✅ I/O-free operation (returns dict)

### Platform Generators (`test_platform_generators.py`)
- ✅ Platform registry (get_platform_generator)
- ✅ Baremetal generator stub
- ✅ STM32 HAL generator (complete)
  - ADC ISR generation
  - SPI ISR generation
  - CAN ISR generation with PGN filtering
  - Peripheral initialization
  - Power-efficient main loop (__WFI)
- ✅ Multi-platform support verification

### UDS Generator (`test_uds_generator.py`)
- ✅ DID define generation
- ✅ Handler function generation
- ✅ Switch-case structure
- ✅ Read/write support
- ✅ Multiple exposure handling

### Integration Tests (`test_integration.py`)
- ✅ Complete pipeline (parse → resolve → validate → generate)
- ✅ I/O-free generators
- ✅ Orchestrator pattern
- ✅ Atomic write simulation
- ✅ UDS integration

## Fixtures

Common fixtures defined in `conftest.py`:

- `sample_adc_node` - ADC hardware input node
- `sample_spi_node` - SPI hardware input node
- `sample_can_node` - CAN hardware input node
- `sample_scale_node` - Scale processing node
- `sample_fault_monitor_node` - Fault monitor node
- `sample_nodes` - Collection of sample nodes
- `sample_resource_counts` - Sample resource count dict

## Key Testing Principles

### 1. I/O-Free Testing
All generators return `dict[str, str]` - no file I/O needed for testing:

```python
def test_generator_returns_dict():
    gen = ConfigGenerator()
    result = gen.generate(nodes, counts)
    assert isinstance(result, dict)
    assert 'lq_config.h' in result
```

### 2. Content Verification
Tests verify generated code contains required elements:

```python
def test_stm32_generates_isr():
    gen = STM32Generator()
    result = gen.generate([adc_node])
    assert 'HAL_ADC_ConvCpltCallback' in result['lq_platform_hw.c']
```

### 3. Integration Testing
End-to-end tests verify complete pipeline:

```python
def test_complete_pipeline():
    nodes = simple_dts_parser(dts)
    nodes = resolve_phandles_and_assign_ids(nodes)
    validate_all(nodes)
    counts = calculate_resource_counts(nodes)
    
    outputs = {}
    outputs.update(ConfigGenerator().generate(nodes, counts))
    outputs.update(PlatformGenerator('stm32').generate(nodes, counts))
    
    # All files generated successfully
    assert 'lq_config.h' in outputs
    assert 'lq_platform_hw.c' in outputs
```

## Example Test Run

```bash
$ pytest tests/ -v

tests/test_config_generator.py::TestConfigGenerator::test_generate_requires_counts PASSED
tests/test_config_generator.py::TestConfigGenerator::test_generate_returns_dict PASSED
tests/test_dts_parser.py::TestParsePropertyValue::test_parse_integer PASSED
tests/test_dts_parser.py::TestParsePropertyValue::test_parse_phandle PASSED
tests/test_platform_generators.py::TestSTM32Generator::test_adc_node_generates_isr PASSED
tests/test_platform_generators.py::TestSTM32Generator::test_can_node_generates_isr PASSED
tests/test_integration.py::TestGeneratorPipeline::test_simple_dts_complete_pipeline PASSED

========================= 50 passed in 1.23s =========================
```

## Adding New Tests

When adding a new generator:

1. Create test file: `tests/test_myfeature_generator.py`
2. Define test class: `class TestMyFeatureGenerator:`
3. Test basic operation:
   - Returns dict
   - Has required files
   - Has required content
4. Test edge cases:
   - Empty input
   - Invalid input
   - Multiple nodes
5. Add integration test in `test_integration.py`

Example:

```python
class TestMyFeatureGenerator:
    def test_generate_returns_dict(self):
        gen = MyFeatureGenerator()
        result = gen.generate([])
        assert isinstance(result, dict)
    
    def test_generated_content(self):
        gen = MyFeatureGenerator()
        result = gen.generate([sample_node])
        assert 'expected_output.c' in result
        assert 'expected_function()' in result['expected_output.c']
```

## Benefits of This Test Suite

1. **No File Mocking** - Pure function testing with string comparisons
2. **Fast Execution** - No I/O operations
3. **Comprehensive Coverage** - Parser, validators, all generators
4. **Easy to Extend** - Add new tests as features are added
5. **Integration Tests** - Verify complete pipeline works end-to-end
6. **Regression Prevention** - Catch breaking changes early

## CI/CD Integration

Add to `.github/workflows/test.yml`:

```yaml
- name: Run generator tests
  run: |
    cd scripts
    pytest tests/ -v --cov=. --cov-report=xml
```
