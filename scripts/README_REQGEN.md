# Requirements-Driven Code Generator (reqgen.py)

## Overview

`reqgen.py` is a tool for generating Devicetree (DTS) configuration, HIL tests, and documentation from structured requirements. It enables requirements-driven development where stakeholders write requirements and technical engineers translate them into implementation details.

## Build Flow

```
Requirements (Natural Language + YAML)
         ↓
    [reqgen.py]          ← This tool
         ↓
    app.dts
         ↓
    [dts_gen.py]         ← Existing tool
         ↓
    lq_generated.c/h
```

## Quick Start

```bash
# From project root
cd my_project/

# Create requirements structure
mkdir -p requirements/{high-level,low-level}

# Write requirements (see examples below)

# Validate requirements
python3 ../modules/layered-queue-driver/scripts/reqgen.py requirements/ validate

# Generate DTS
python3 ../modules/layered-queue-driver/scripts/reqgen.py requirements/ generate-dts -o app.dts

# Generate C code (use existing dts_gen.py)
python3 ../modules/layered-queue-driver/scripts/dts_gen.py app.dts src/
```

## Requirements Structure

```
requirements/
├── high-level/          # Natural language (non-technical)
│   ├── speed-control.md
│   └── safety-monitoring.md
│
└── low-level/           # Structured YAML (technical)
    ├── llr-0.1-engine-config.yaml
    ├── llr-1.1-adc-sampling.yaml
    └── llr-1.2-speed-scaling.yaml
```

## High-Level Requirements (HLR)

Written in markdown by non-technical stakeholders:

```markdown
# HLR-1: Motor Speed Control

**ID:** HLR-1
**Priority:** Critical
**Stakeholder:** Operations Team

## Requirement
The system shall control motor speed based on potentiometer input.

## Details
- Input: 0-5V analog
- Output: 0-3000 RPM
- Response time: < 100ms

## Verification
- HIL test: Verify response time
- HIL test: Verify accuracy ±5%
```

## Low-Level Requirements (LLR)

Written in YAML by technical engineers:

```yaml
id: LLR-1.1
parent: HLR-1
title: ADC Sampling
description: Sample analog speed setpoint from potentiometer

implementation:
  node_type: lq,hw-adc-input
  node_name: adc1_speed_input
  properties:
    io-channels: <&adc1 1>
    min-raw: 0
    max-raw: 4095
    stale-us: 100000

verification:
  - method: hil_test
    criteria: "Inject voltage → verify ADC reading"
```

## Node Merging

**Multiple LLRs can configure the same device node:**

```yaml
# llr-0.2-motor-device.yaml
node_name: motor
properties:
  node-id: 2
  can-device: <&can1>

# llr-0.3-motor-eds.yaml
node_name: motor        # SAME NODE!
properties:
  eds: "sensor_device.eds"
```

**Generates merged node:**

```dts
/* motor - configured by: LLR-0.2, LLR-0.3 */
motor: motor {
    compatible = "lq,canopen-device";
    node-id = <2>;
    can-device = <&can1>;
    eds = "sensor_device.eds";
}
```

This is useful for separating concerns:
- One LLR for device identity (node-id, bus)
- Another LLR for configuration (EDS file, parameters)
- Another LLR for timing (cycle-time, heartbeat)

## Conflict Detection

The tool automatically detects conflicts:

### ❌ ERROR (Blocks Generation)

**Resource Conflicts:**
```
ERROR: LLR-1.1 and LLR-2.1 both use ADC1 channel 1
```

**Timing Conflicts:**
```
ERROR: HLR-1 requires <50ms but implementation takes 80ms
```

**Dependency Conflicts:**
```
ERROR: Circular dependency: LLR-1.2 → LLR-1.3 → LLR-1.2
```

### ⚠️ WARNING (Allows Generation)

**Budget Exceeded:**
```
WARNING: 35 signals used, limit is 32
Auto-fix available: Increase max-signals
```

**Property Conflicts:**
```
WARNING: Property conflict on motor.node-id
  LLR-0.2: 2
  LLR-0.3: 3
Using value from LLR-0.3
```

## Command Reference

```bash
# Validate requirements
python3 scripts/reqgen.py requirements/ validate

# Validate with auto-fix
python3 scripts/reqgen.py requirements/ validate --auto-fix

# Validate in strict mode (warnings = errors)
python3 scripts/reqgen.py requirements/ validate --strict

# Generate DTS
python3 scripts/reqgen.py requirements/ generate-dts -o app.dts

# Force generate (ignore errors - not recommended)
python3 scripts/reqgen.py requirements/ generate-dts -o app.dts --force

# Generate HIL tests
python3 scripts/reqgen.py requirements/ generate-tests -o tests/hil/

# Generate documentation
python3 scripts/reqgen.py requirements/ generate-docs -o REQUIREMENTS.md

# Generate everything
python3 scripts/reqgen.py requirements/ all
```

## Supported Node Types

All layered-queue-driver bindings are supported:

- `lq,engine` - Engine configuration
- `lq,hw-adc-input` - ADC input
- `lq,hw-sensor-input` - Sensor input
- `lq,scale` - Scaling/conversion
- `lq,verified-output` - Verified output
- `lq,cyclic-output` - Periodic CAN output
- `lq,fault-monitor` - Fault monitoring
- `lq,gpio-pattern` - LED patterns
- `lq,canopen-device` - CANopen devices
- `lq,pid` - PID controller
- `lq,merge` / `lq,voter` - Signal merging
- And all others in `dts/bindings/layered-queue/`

## Dependencies

- Python 3.7+
- PyYAML (`pip install pyyaml`)

Optional:
- For JSON requirements: Python stdlib only (no PyYAML needed)

## Integration with CI/CD

```yaml
# .github/workflows/validate-requirements.yml
name: Validate Requirements

on: [push, pull_request]

jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: pip install pyyaml

      - name: Validate requirements
        run: |
          python3 modules/layered-queue-driver/scripts/reqgen.py \
            requirements/ validate --strict

      - name: Generate DTS
        run: |
          python3 modules/layered-queue-driver/scripts/reqgen.py \
            requirements/ generate-dts -o app.dts

      - name: Check generated files
        run: |
          git diff --exit-code app.dts
          # Fails if generated DTS doesn't match committed version
```

## Examples

See `../../examples/requirements/` for complete examples including:
- Motor speed control with verification
- Multi-sensor voting system
- Fault monitoring with limp-home
- CANopen device configuration

## Relationship to Other Scripts

- **dts_gen.py**: Generates C code FROM DTS (already exists)
- **reqgen.py**: Generates DTS FROM requirements (this tool)
- **canopen_eds_parser.py**: Parses EDS files
- **hil_test_gen.py**: Generates HIL test infrastructure

Together they form a complete toolchain:
```
Requirements → DTS → C Code + Tests
```

## Development Mode vs Production Mode

**Development (Lenient):**
```bash
python3 scripts/reqgen.py requirements/ validate --auto-fix
python3 scripts/reqgen.py requirements/ generate-dts -o app.dts
```

**Code Review (Strict):**
```bash
python3 scripts/reqgen.py requirements/ validate --strict
# Treats warnings as errors
```

**Production (Error-free only):**
```bash
python3 scripts/reqgen.py requirements/ validate
# Only ERROR-level conflicts block generation
```

## Benefits

### For Non-Technical Stakeholders
✅ Write requirements in natural language
✅ No need to understand DTS syntax
✅ Clear verification criteria

### For Technical Engineers
✅ Structured implementation details
✅ Automatic conflict detection
✅ Traceability to HLRs
✅ Node merging for separation of concerns

### For System Integration
✅ Automatic code generation
✅ Consistency validation
✅ HIL test generation
✅ Documentation generation

### For Safety/Compliance
✅ Full traceability matrix
✅ Requirement coverage checking
✅ Conflict documentation
✅ Audit trail

## FAQ

**Q: Can multiple LLRs configure the same node?**
A: Yes! This is a key feature. Properties are merged, conflicts detected.

**Q: What happens if requirements conflict?**
A: Conflicts are detected and categorized (ERROR/WARNING/INFO). ERRORs block generation.

**Q: Can I use JSON instead of YAML?**
A: Yes, the tool can be extended to support JSON LLRs.

**Q: Do I need to learn DTS syntax?**
A: No! LLRs abstract DTS syntax. But understanding helps debugging.

**Q: What if I don't want requirements-driven development?**
A: This tool is optional. You can write DTS directly as before.

**Q: Can this generate for other frameworks?**
A: Currently layered-queue-driver only, but designed to be extensible.

## Support

For issues or questions:
1. Check examples in `../../examples/requirements/`
2. Read binding documentation in `../dts/bindings/`
3. Open issue in layered-queue-driver repository
