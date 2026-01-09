# CANopen BLDC Motor Driver Example

This example demonstrates a complete BLDC motor controller using CANopen protocol with automatic code generation from EDS files.

## Quick Start

Simply build the example - all code generation happens automatically:

```bash
cmake --build build --target canopen_motor_driver
./build/samples/canopen/canopen_motor_driver
```

## Key Features

- **Declarative System Description**: Single DTS file describes entire system
- **EDS Inline Expansion**: CANopen PDO mappings auto-generated from EDS at build time
- **Signal ID Header**: motor_signals.h with named constants (SIG_CONTROLWORD, etc.)
- **Platform Abstraction**: Works on native, FreeRTOS, and Zephyr

## Architecture

The build system uses **inline EDS expansion**:

```
User Files:
├── motor_system.dts     ← Your system description (includes canopen node)
└── example_motor.eds    ← CANopen device definition

Build Process:
├── Step 1: Expand EDS inline → expanded.dts + motor_signals.h
└── Step 2: Generate code → lq_generated.c/h + main.c

Generated Files:
├── expanded.dts          (DTS with full CANopen PDO mappings)
├── motor_signals.h       (Signal ID constants: SIG_STATUSWORD, etc.)
├── lq_generated.c/h      (Engine initialization and PDO handlers)
└── main.c                (Platform-agnostic application entry point)
```

## Device Tree Structure

The `motor_system.dts` file declares both the CANopen protocol and motor hardware:

```dts
/ {
    /* CANopen with EDS reference */
    canopen: canopen-device@1 {
        compatible = "lq,protocol-canopen";
        eds = "example_motor.eds";  /* Auto-expands at build time */
        node-id = <5>;  /* Can override EDS defaults */
    };
    
    /* BLDC motor configuration */
    bldc_motor: bldc-output@0 {
        compatible = "lq,verified-output";
        pole-pairs = <7>;
        max-rpm = <10000>;
        /* ... motor parameters ... */
    };
};
```

## How It Works

### Build-Time Code Generation

The [CMakeLists.txt](CMakeLists.txt) uses a two-stage build:

```cmake
# Stage 1: Expand EDS references inline
add_custom_command(
    OUTPUT expanded.dts motor_signals.h
    COMMAND scripts/dts_gen.py 
            motor_system.dts
            build/samples/canopen
            --expand-eds
            --signals-header motor_signals.h
    DEPENDS motor_system.dts example_motor.eds
)

# Stage 2: Generate lq_generated.c/h and main.c
add_custom_command(
    OUTPUT lq_generated.c lq_generated.h main.c
    COMMAND scripts/dts_gen.py
            expanded.dts
            build/samples/canopen
            --platform=baremetal
    DEPENDS expanded.dts
)
```

This ensures:
- **Automatic updates**: Modify `motor_system.dts` or `example_motor.eds` and everything regenerates
- **Type safety**: Signal IDs are compile-time constants
- **Single source of truth**: One DTS file declares the entire system
- **No manual merge**: EDS expands inline automatically

### Generated Files

**build/samples/canopen/motor_signals.h:**
```c
/* Auto-generated CANopen signal IDs - DO NOT EDIT */
#define SIG_CONTROLWORD           0  /* RPDO1: Controlword */
#define SIG_MODES_OF_OPERATION    1  /* RPDO1: Modes of Operation */
#define SIG_STATUSWORD          100  /* TPDO1: Statusword */
#define SIG_VELOCITY_ACTUAL_VALUE 101  /* TPDO1: Velocity Actual Value */
```

**build/samples/canopen/motor.dts:**
```dts
canopen_device: canopen-device@5 {
    compatible = "lq,protocol-canopen";
    node-id = <5>;
    tpdo1: tpdo@0 {
        mapping@0 { index = <0x6041>; signal-id = <100>; };
        // ...
    };
};
```

## Customizing for Your Device

### 1. Replace the EDS File

Put your vendor-supplied EDS file in this directory:

```bash
cp ~/Downloads/my_motor_controller.eds samples/canopen/
```

### 2. Update CMakeLists.txt

Change the EDS_FILE path:

```cmake
set(EDS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/my_motor_controller.eds")
```

### 3. Rebuild

```bash
cmake --build build --target canopen_motor_driver
```

All signal IDs and device tree mappings regenerate automatically!

## 3-Phase BLDC Motor Example

The included `example_motor.eds` describes a BLDC motor driver with:

| Object | Index  | Name                    | RPDO/TPDO |
|--------|--------|-------------------------|-----------|
| 0x6040 | 0      | Controlword             | RPDO      |
| 0x6041 | 0      | Statusword              | TPDO      |
| 0x6060 | 0      | Modes of Operation      | RPDO      |
| 0x606C | 0      | Velocity Actual Value   | TPDO      |
| 0x6502 | 0      | Supported Drive Modes   | TPDO      |

## References

- CANopen Standard: CiA 301 (Application Layer)
- Drive Profile: CiA 402 (Device Profile for Drives)
- [EDS Parser Documentation](../../scripts/canopen_eds_parser.py)
- [CANopen Protocol Implementation](../../include/lq_canopen.h)
