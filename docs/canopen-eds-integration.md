# CANopen EDS Integration - Complete Workflow

This document describes the EDS inline expansion feature that allows users to reference CANopen EDS files directly in their device tree.

## Overview

Instead of manually managing CANopen PDO mappings, users declare a CANopen node in their DTS and reference an EDS file. The build system automatically expands the full PDO configuration inline.

## User Perspective

### What You Write (motor_system.dts)

```dts
/ {
    canopen: canopen-device@1 {
        compatible = "lq,protocol-canopen";
        eds = "example_motor.eds";  /* This triggers expansion */
        node-id = <5>;  /* Optional: override EDS default */
    };
    
    bldc_motor: bldc-output@0 {
        compatible = "lq,verified-output";
        /* ... motor configuration ... */
    };
};
```

### What Gets Generated

The build system automatically creates:

1. **expanded.dts** - Your DTS with full CANopen PDO mappings
2. **motor_signals.h** - Signal ID constants (SIG_CONTROLWORD, etc.)
3. **lq_generated.c/h** - Engine initialization code
4. **main.c** - Platform-agnostic application entry point

## Build Pipeline

```
┌─────────────────────────────────────────────────────────────┐
│ User Files                                                  │
├─────────────────────────────────────────────────────────────┤
│  motor_system.dts     → System description with EDS ref    │
│  example_motor.eds    → CANopen device definition           │
└─────────────────────────────────────────────────────────────┘
                              ↓
                    ┌────────────────────┐
                    │  Stage 1: Expand   │
                    │  EDS References    │
                    └────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│ Expanded Files                                              │
├─────────────────────────────────────────────────────────────┤
│  expanded.dts         → Full CANopen node with TPDO/RPDO  │
│  motor_signals.h      → Signal ID constants                │
└─────────────────────────────────────────────────────────────┘
                              ↓
                    ┌────────────────────┐
                    │  Stage 2: Generate │
                    │  C Code            │
                    └────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│ Generated Code                                              │
├─────────────────────────────────────────────────────────────┤
│  lq_generated.c/h     → Engine initialization              │
│  main.c               → Application entry point            │
└─────────────────────────────────────────────────────────────┘
                              ↓
                    ┌────────────────────┐
                    │  Stage 3: Compile  │
                    └────────────────────┘
                              ↓
                  canopen_motor_driver (executable)
```

## EDS Expansion Details

### Input: DTS with EDS Reference

```dts
canopen: canopen-device@1 {
    compatible = "lq,protocol-canopen";
    eds = "example_motor.eds";
    node-id = <5>;
};
```

### Output: Expanded DTS

```dts
canopen: canopen-device@1 {
    compatible = "lq,protocol-canopen";
    node-id = <5>;
    label = "BLDC Motor Driver 3000";
    
    /* Auto-generated from EDS file */
    
    tpdo1: tpdo@0 {
        cob-id = <389>;  /* 0x180 + 0x100 * 0 + 5 */
        
        mapping@0 {
            index = <24641>;     /* 0x6041 - Statusword */
            subindex = <0>;
            length = <16>;
            signal-id = <100>;   /* SIG_STATUSWORD */
        };
        
        mapping@1 {
            index = <24684>;     /* 0x606C - Velocity Actual */
            subindex = <0>;
            length = <32>;
            signal-id = <101>;   /* SIG_VELOCITY_ACTUAL_VALUE */
        };
    };
    
    rpdo1: rpdo@0 {
        cob-id = <517>;  /* 0x200 + 0x100 * 0 + 5 */
        
        mapping@0 {
            index = <24640>;     /* 0x6040 - Controlword */
            subindex = <0>;
            length = <16>;
            signal-id = <0>;     /* SIG_CONTROLWORD */
        };
    };
};
```

## Implementation

### CMakeLists.txt

```cmake
# Stage 1: Expand EDS references in DTS
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/expanded.dts
           ${CMAKE_CURRENT_BINARY_DIR}/motor_signals.h
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
        ${CMAKE_CURRENT_SOURCE_DIR}/motor_system.dts
        ${CMAKE_CURRENT_BINARY_DIR}
        --expand-eds
        --signals-header ${CMAKE_CURRENT_BINARY_DIR}/motor_signals.h
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
            ${CMAKE_SOURCE_DIR}/scripts/canopen_eds_parser.py
            ${CMAKE_CURRENT_SOURCE_DIR}/motor_system.dts
            ${CMAKE_CURRENT_SOURCE_DIR}/example_motor.eds
    COMMENT "Expanding EDS references in device tree"
)

# Stage 2: Generate lq_generated.c/h and main.c
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lq_generated.c
           ${CMAKE_CURRENT_BINARY_DIR}/lq_generated.h
           ${CMAKE_CURRENT_BINARY_DIR}/main.c
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
        ${CMAKE_CURRENT_BINARY_DIR}/expanded.dts
        ${CMAKE_CURRENT_BINARY_DIR}
        --platform=baremetal
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
            ${CMAKE_CURRENT_BINARY_DIR}/expanded.dts
    COMMENT "Generating code from device tree"
)
```

### dts_gen.py Expansion Logic

```python
def expand_eds_references(input_dts_path, output_dts_path, signals_header_path):
    """Find CANopen nodes with 'eds' property and expand them"""
    
    # Read DTS
    with open(input_dts_path, 'r') as f:
        dts_content = f.read()
    
    # Find canopen nodes with eds="file.eds"
    for node with eds property:
        # Parse EDS file
        eds_data = parse_eds_file(eds_path)
        
        # Generate full CANopen node with TPDO/RPDO
        expanded_node = generate_canopen_node(eds_data, label, address)
        
        # Replace stub node with expanded version
        dts_content = dts_content.replace(original_node, expanded_node)
    
    # Write expanded DTS
    with open(output_dts_path, 'w') as f:
        f.write(dts_content)
    
    # Generate signal header
    generate_canopen_signal_header(tpdos, rpdos, signals_header_path)
```

## Benefits

### 1. Single Source of Truth

User writes **one** DTS file that declares everything:
- CANopen protocol configuration
- Motor hardware parameters
- Safety limits and features

### 2. Automatic Updates

Change `example_motor.eds` → everything regenerates automatically:
- Expanded DTS
- Signal ID header
- Generated code
- Compiled executable

### 3. Clean Separation

```
User's Responsibility:
  ✓ Write motor_system.dts (declares what you want)
  ✓ Provide example_motor.eds (from motor vendor)

Build System's Responsibility:
  ✓ Parse EDS file
  ✓ Expand PDO mappings inline
  ✓ Generate signal IDs
  ✓ Generate initialization code
  ✓ Generate application entry point
```

### 4. Override Support

Users can override EDS defaults:

```dts
canopen: canopen-device@1 {
    eds = "example_motor.eds";
    node-id = <10>;  /* Override default node ID */
    /* COB IDs automatically recalculated */
};
```

### 5. Type Safety

Signal IDs are compile-time constants:

```c
#include "motor_signals.h"

// Compile error if signal doesn't exist
lq_engine_set_signal(&g_lq_engine, SIG_CONTROLWORD, 0x000F);
```

## Signal ID Allocation

### RPDO (Commands from master)

- Signal ID range: 0-99
- Formula: `pdo_idx * 32 + mapping_idx`

```
RPDO1[0]: SIG_CONTROLWORD          =   0
RPDO1[1]: SIG_MODES_OF_OPERATION   =   1
RPDO2[0]: SIG_TARGET_VELOCITY      =  32
RPDO2[1]: SIG_TARGET_TORQUE        =  33
```

### TPDO (Status to master)

- Signal ID range: 100-199
- Formula: `100 + pdo_idx * 32 + mapping_idx`

```
TPDO1[0]: SIG_STATUSWORD                  = 100
TPDO1[1]: SIG_VELOCITY_ACTUAL_VALUE       = 101
TPDO2[0]: SIG_POSITION_ACTUAL_VALUE       = 132
TPDO2[1]: SIG_CURRENT_ACTUAL_VALUE        = 133
```

## Related Files

### Scripts
- `scripts/dts_gen.py` - DTS parser and code generator
  - `expand_eds_references()` - Find and expand EDS references
  - `generate_canopen_node()` - Generate full CANopen DTS node
  - `generate_canopen_signal_header()` - Generate signal ID header

- `scripts/canopen_eds_parser.py` - EDS file parser
  - `parse_eds_file()` - Parse EDS and return dict
  - `parse_eds()` - Low-level EDS parser (CANopenDevice object)

### Build Files
- `samples/canopen/CMakeLists.txt` - Build pipeline with two stages

### User Files
- `samples/canopen/motor_system.dts` - System description
- `samples/canopen/example_motor.eds` - CANopen device definition

### Generated Files (build/samples/canopen/)
- `expanded.dts` - DTS with expanded CANopen nodes
- `motor_signals.h` - Signal ID constants
- `lq_generated.c/h` - Engine initialization
- `main.c` - Application entry point

## Future Enhancements

### Multiple CANopen Devices

```dts
/ {
    motor1: canopen-device@1 {
        eds = "motor_axis1.eds";
        node-id = <5>;
    };
    
    motor2: canopen-device@2 {
        eds = "motor_axis2.eds";
        node-id = <6>;
    };
    
    io_module: canopen-device@3 {
        eds = "digital_io.eds";
        node-id = <7>;
    };
};
```

### Conditional PDO Mapping

```dts
canopen: canopen-device@1 {
    eds = "example_motor.eds";
    enable-tpdo1;    /* Only enable specific TPDOs */
    enable-tpdo2;
    disable-rpdo3;   /* Disable unused RPDOs */
};
```

### Custom Signal ID Ranges

```dts
canopen: canopen-device@1 {
    eds = "example_motor.eds";
    rpdo-signal-base = <200>;  /* Start RPDOs at 200 instead of 0 */
    tpdo-signal-base = <300>;  /* Start TPDOs at 300 instead of 100 */
};
```
