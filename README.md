# Layered Queue Driver for Zephyr RTOS

[![Build and Test](https://github.com/and-elf/layered-queue-driver/actions/workflows/test.yml/badge.svg)](https://github.com/and-elf/layered-queue-driver/actions/workflows/test.yml)
[![Coverage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/and-elf/layered-queue-driver/main/.github/badges/coverage.json)](https://github.com/and-elf/layered-queue-driver/actions/workflows/coverage-badge.yml)

A declarative, device-tree-driven framework for building robust data pipelines in safety-critical embedded systems. The Layered Queue Driver enables hardware sensor abstraction, range validation, redundancy management, and fault detection—all configured through device tree.

## Features

- **Declarative Configuration**: Define entire data pipelines in device tree
- **Hardware Abstraction**: ADC, SPI, GPIO sources with unified queue interface
- **Range Validation**: Per-source value range checking with status classification
- **Redundancy Management**: Merge/vote on multiple redundant inputs with configurable algorithms
- **Fault Detection**: Dual-inverted inputs, tolerance checking, timeout detection
- **Safety-Critical Ready**: Thread-safe queues, deterministic behavior, error propagation
- **Statistics**: Built-in monitoring for drops, peak usage, throughput
- **Multiple Output Types**: GPIO, PWM, DAC, SPI, I2C, UART, CAN, J1939, CANopen, Modbus

## Architecture

The Layered Queue Driver uses a clean layered architecture separating hardware concerns from pure processing logic:

```
┌─────────────────────────────────────────────┐
│         Hardware ISR / Polling              │ ← RTOS-aware, minimal
│  (ADC callbacks, SPI reads, GPIO events)    │
└─────────────────┬───────────────────────────┘
                  │ lq_hw_push()
┌─────────────────▼───────────────────────────┐
│      Layer 2: Input Aggregator              │ ← RTOS-aware, thin
│         (Hardware ringbuffer)               │
└─────────────────┬───────────────────────────┘
                  │ lq_hw_pop()
┌─────────────────▼───────────────────────────┐
│    Mid-level Drivers (vtable pattern)       │ ← PURE (no RTOS)
│  • ADC validator                            │
│  • SPI validator                            │
│  • Merge/Voter                              │
│  • Range checker                            │
└─────────────────┬───────────────────────────┘
                  │ events
┌─────────────────▼───────────────────────────┐
│         ENGINE STEP (pure, once)            │ ← Deterministic
│    Process all inputs → Generate events     │
└─────────────────┬───────────────────────────┘
                  │ events
┌─────────────────▼───────────────────────────┐
│          Output Drivers                     │ ← Hardware adapters
│  • CAN • J1939 • CANopen • GPIO             │
│  • PWM • DAC • SPI • I2C • UART • Modbus    │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│       Sync Groups (atomic updates)          │
└─────────────────────────────────────────────┘
```

**Key Benefits:**
- **Testability**: Pure mid-level drivers and engine step are easily unit tested
- **Portability**: Hardware abstraction enables running on any platform
- **Determinism**: Single engine step processes all inputs predictably
- **Safety**: Clear separation enables formal verification of pure layers

## Quick Start

### 1. Hardware Layer - Capture Raw Samples

Connect hardware interrupts to the input aggregator:

```c
#include "lq_hw_input.h"

// ADC interrupt callback
void adc_isr_callback(uint16_t sample)
{
    lq_hw_push(LQ_HW_ADC0, sample);  // ISR-safe
}

// SPI polling task
void spi_poll_task(void)
{
    while (1) {
        uint32_t value = spi_read_sensor();
        lq_hw_push(LQ_HW_SPI0, value);
        k_sleep(K_MSEC(10));
    }
}
```

### 2. Device Tree Configuration

Define the complete pipeline in device tree (see [samples/automotive/app.dts](samples/automotive/app.dts) for complete example):

```dts
/ {
    /* Engine configuration */
    engine: lq-engine {
        compatible = "lq,engine";
        max-signals = <32>;
        max-merges = <8>;
        max-cyclic-outputs = <16>;
    };
    
    /* Hardware inputs - ISRs auto-generated */
    rpm_adc: lq-hw-adc-input@0 {
        compatible = "lq,hw-adc-input";
        signal-id = <0>;
        adc-channel = <0>;
        stale-us = <5000>;
    };
    
    rpm_spi: lq-hw-spi-input@0 {
        compatible = "lq,hw-spi-input";
        signal-id = <1>;
        spi-device = <&spi0>;
        stale-us = <5000>;
    };
    
    /* Redundancy management */
    rpm_merge: lq-mid-merge@0 {
        compatible = "lq,mid-merge";
        output-signal-id = <10>;
        input-signal-ids = <0 1>;
        voting-method = "median";
        tolerance = <50>;
    };
    
    /* Cyclic outputs - dispatch auto-generated */
    rpm_output: lq-cyclic-output@0 {
        compatible = "lq,cyclic-output";
        source-signal-id = <10>;
        output-type = "j1939";
        target-id = <0xFEF1>;  /* J1939 PGN */
        period-us = <100000>;  /* 10Hz */
    };
        
        gpio_warning: lq-output@1 {
            compatible = "lq,gpio-output";
            source = <&engine_sensor>;
            pin = <5>;
            trigger-status = <1>;  /* >= DEGRADED */
        };
    };
    
    /* Synchronized updates */
    sync_groups {
        snapshot: lq-sync-group@0 {
            period-us = <100000>;
            members = <&can_speed &gpio_warning>;
        };
    };
};
```

### 3. Code Generation and Main Loop

Generate C code from device tree:

```bash
python3 scripts/dts_gen.py app.dts src/
# Generates: src/lq_generated.h, src/lq_generated.c
```

The main application loop is simple:

```c
#include "lq_generated.h"
#include "lq_platform.h"

int main(void)
{
    // Initialize generated engine configuration
    lq_generated_init();
    
    // Main processing loop
    while (1) {
        uint64_t now_us = lq_platform_get_time_us();
        
        // Process all inputs, run voting/merging, generate outputs
        lq_engine_step(&g_lq_engine, now_us);
        
        // Dispatch outputs to hardware (auto-generated based on DTS)
        lq_generated_dispatch_outputs();
        
        lq_platform_sleep_ms(10);  // 100Hz processing rate
    }
}
```

**What happens inside `lq_engine_step()`:**
1. Pops pending hardware samples from ISR ringbuffer
2. Updates signal values with staleness checking
3. Runs merge/voting algorithms on redundant inputs
4. Generates cyclic output events based on deadlines
5. Generates on-change output events for threshold triggers

**What happens inside `lq_generated_dispatch_outputs()`:**
1. Iterates through output events buffer
2. Routes each event to appropriate protocol driver
3. Encodes values (J1939 PGN, CANopen COB-ID, etc.)
4. Calls platform functions (lq_can_send, lq_gpio_set, etc.)

Both functions are **pure processing** - no RTOS dependencies, easily testable.

## Device Tree Node Types

### Hardware Input Nodes

**`lq,hw-adc-input`** - ADC sensor input with ISR callback
- `signal-id`: Target signal array index
- `adc-channel`: ADC hardware channel
- `adc-device`: Device reference (Zephyr only)
- `isr-priority`: Interrupt priority
- `stale-us`: Staleness timeout in microseconds

**`lq,hw-spi-input`** - SPI sensor input with data-ready GPIO
- `signal-id`: Target signal array index
- `spi-device`: SPI bus reference
- `data-ready-gpio`: GPIO for data ready interrupt
- `num-bytes`: Number of bytes to read
- `signed`: true if value is signed
- `stale-us`: Staleness timeout

**`lq,hw-gpio-input`** - Digital input monitoring
- `signal-id`: Target signal array index
- `gpio-pin`: GPIO pin reference
- `debounce-us`: Debounce time
- `stale-us`: Staleness timeout

### Mid-Level Processing Nodes

**`lq,mid-merge`** - Redundant input voting/merging
- `output-signal-id`: Merged result signal ID
- `input-signal-ids`: Array of input signal IDs
- `voting-method`: `median`, `average`, `min`, `max`
- `tolerance`: Maximum deviation between inputs
- `stale-us`: Staleness timeout

**`lq,mid-remap`** - Value remapping/lookup table
- `input-signal-id`: Source signal
- `output-signal-id`: Destination signal
- `map`: Array of [input, output] pairs

**`lq,mid-scale`** - Linear scaling transformation
- `input-signal-id`: Source signal
- `output-signal-id`: Destination signal  
- `scale`: Multiplication factor (Q16.16 fixed point)
- `offset`: Addition offset

### Output Nodes

**`lq,cyclic-output`** - Periodic output transmission
- `source-signal-id`: Source signal to transmit
- `output-type`: Protocol type (see Output Types below)
- `target-id`: Protocol-specific address (PGN, COB-ID, pin, etc.)
- `period-us`: Transmission period in microseconds
- `priority`: Scheduling priority (lower = higher priority)

**Output Types:** `can`, `j1939`, `canopen`, `gpio`, `uart`, `spi`, `i2c`, `pwm`, `dac`, `modbus`

See [docs/output-types-reference.md](docs/output-types-reference.md) for complete output type documentation.

## Signal Status Codes

Every signal value has an associated status indicating data quality:

```c
enum lq_event_status {
    LQ_EVENT_OK = 0,              // Normal operation
    LQ_EVENT_DEGRADED = 1,        // Degraded but functional
    LQ_EVENT_OUT_OF_RANGE = 2,    // Out of acceptable range
    LQ_EVENT_ERROR = 3,           // Hardware/communication error
    LQ_EVENT_TIMEOUT = 4,         // Data staleness timeout
    LQ_EVENT_INCONSISTENT = 5,    // Redundant inputs disagree (exceeds tolerance)
};
```

**Status Propagation:**
- Hardware inputs start with `LQ_EVENT_OK`
- Timeout detection upgrades to `LQ_EVENT_TIMEOUT` when stale
- Merge nodes upgrade to `LQ_EVENT_INCONSISTENT` when inputs disagree
- Range checkers upgrade to `LQ_EVENT_OUT_OF_RANGE` when out of bounds
- Status flows through to output events for monitoring

## Directory Structure

```
layered-queue-driver/
├── CMakeLists.txt                 # Build configuration
├── Kconfig                        # KConfig options (Zephyr integration)
├── west.yml                       # Zephyr west manifest
├── scripts/
│   ├── dts_gen.py                 # Device tree → C code generator
│   ├── hil_test_gen.py            # HIL test generator
│   └── platform_adaptors.py      # Platform adapter generator
├── include/                       # Public API headers
│   ├── lq_engine.h                # Engine core API
│   ├── lq_event.h                 # Event types and output drivers
│   ├── lq_hw_input.h              # Hardware input layer (ISR-safe)
│   ├── lq_platform.h              # Platform abstraction
│   ├── lq_j1939.h                 # J1939 protocol support
│   ├── lq_canopen.h               # CANopen protocol support
│   ├── lq_pid.h                   # PID controller
│   ├── lq_dtc.h                   # Diagnostic Trouble Codes
│   └── layered_queue_core.h       # Core queue utilities
├── src/drivers/                   # Core driver implementations
│   ├── lq_engine.c                # Pure engine processing
│   ├── lq_hw_input.c              # Hardware input aggregator
│   ├── lq_j1939.c                 # J1939 implementation
│   ├── lq_canopen.c               # CANopen implementation
│   ├── lq_pid.c                   # PID controller
│   ├── lq_dtc.c                   # DTC management
│   └── platform/
│       ├── lq_platform_native.c   # Native/POSIX platform
│       ├── lq_platform_stubs.c    # Default platform stubs
│       └── lq_platform_freertos.c # FreeRTOS platform
├── samples/                       # Example applications
│   ├── automotive/                # Automotive example (engine monitor)
│   │   ├── app.dts                # Device tree configuration
│   │   └── src/
│   │       ├── lq_generated.h     # Auto-generated header
│   │       └── lq_generated.c     # Auto-generated implementation
│   ├── multi-output-example.dts   # All 10 output types demo
│   ├── basic/                     # Basic examples
│   ├── freertos/                  # FreeRTOS integration
│   └── stm32/                     # STM32 HAL integration
├── tests/                         # Unit and integration tests
│   ├── engine_test.cpp            # Engine tests
│   ├── queue_test.cpp             # Queue tests
│   ├── hil_test.cpp               # Hardware-in-loop tests
│   └── hil/                       # HIL test infrastructure
├── docs/                          # Documentation
│   ├── architecture.md            # Architecture overview
│   ├── output-types-reference.md  # Output types API reference
│   ├── platform-portability.md    # Cross-compiler guide
│   ├── devicetree-guide.md        # DTS configuration guide
│   └── HIL_TESTING.md             # HIL testing guide
└── dts/                           # Device tree bindings
    ├── bindings/                  # YAML binding definitions
    └── examples/                  # Example DTS files
```
│   ├── basic/                     # Legacy basic example
│   └── layered/                   # New layered architecture example
├── tests/
│   ├── queue_test.cpp             # Queue tests
│   ├── engine_test.cpp            # Engine step tests (pure)
│   └── util/                      # Utility tests
└── docs/
    ├── architecture.md            # Architecture documentation
    ├── devicetree-guide.md        # Device tree guide
    └── testing.md                 # Testing guide
```

## Examples

### Example 1: Automotive System with Multiple Output Types

Complete example showing all 10 output types (CAN, J1939, CANopen, GPIO, UART, SPI, I2C, PWM, DAC, Modbus):

[See samples/multi-output-example.dts](samples/multi-output-example.dts)

### Example 2: Automotive Engine Monitor

Real-world example with dual-redundant RPM sensors, median voting, and J1939 CAN outputs:

[See samples/automotive/app.dts](samples/automotive/app.dts)

### Example 3: Redundant Sensor Merge

Device tree snippet showing redundancy management:

```dts
/* Two ADC sensors reading same parameter */
sensor_a: lq-hw-adc-input@0 {
    compatible = "lq,hw-adc-input";
    signal-id = <0>;
    adc-channel = <0>;
    stale-us = <5000>;
};

sensor_b: lq-hw-adc-input@1 {
    compatible = "lq,hw-adc-input";
    signal-id = <1>;
    adc-channel = <1>;
    stale-us = <5000>;
};

/* Median voting with tolerance check */
merged: lq-mid-merge@0 {
    compatible = "lq,mid-merge";
    output-signal-id = <10>;
    input-signal-ids = <0 1>;
    voting-method = "median";
    tolerance = <50>;  /* Flag if sensors disagree by >50 */
    stale-us = <10000>;
};

/* Output merged value over J1939 */
can_output: lq-cyclic-output@0 {
    compatible = "lq,cyclic-output";
    source-signal-id = <10>;
    output-type = "j1939";
    target-id = <0xFEF1>;  /* Engine speed PGN */
    period-us = <100000>;  /* 10 Hz */
};
```

## Code Generation

The framework uses Python scripts to generate C code from device tree:

```bash
# Generate code
python3 scripts/dts_gen.py input.dts output_dir/

# Generates:
# - lq_generated.h  (declarations)
# - lq_generated.c  (engine config, ISRs, dispatch function)
# - lq_generated_test.dts (HIL test configuration)
```

**Platform-specific generation:**

```bash
# Generate with STM32 HAL integration
python3 scripts/dts_gen.py app.dts src/ --platform=stm32

# Generate with ESP32 integration  
python3 scripts/dts_gen.py app.dts src/ --platform=esp32
```

See [docs/platform-portability.md](docs/platform-portability.md) for cross-compiler support.

## Configuration

Configure in `prj.conf`:

```ini
CONFIG_LQ_DRIVER=y
CONFIG_LQ_MAX_SIGNALS=32
CONFIG_LQ_MAX_CYCLIC_OUTPUTS=16
CONFIG_LQ_MAX_OUTPUT_EVENTS=64
CONFIG_LQ_MAX_MERGES=8
CONFIG_LQ_HW_RINGBUFFER_SIZE=128
CONFIG_LQ_LOG_LEVEL=3
```

### KConfig Options

- `CONFIG_LQ_MAX_SIGNALS`: Maximum number of signals (default: 32)
- `CONFIG_LQ_MAX_CYCLIC_OUTPUTS`: Maximum cyclic outputs (default: 16)
- `CONFIG_LQ_MAX_OUTPUT_EVENTS`: Output event buffer size (default: 64)
- `CONFIG_LQ_MAX_MERGES`: Maximum merge/voter contexts (default: 8)
- `CONFIG_LQ_HW_RINGBUFFER_SIZE`: Hardware ringbuffer size (default: 128)
- `CONFIG_LQ_LOG_LEVEL`: 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug

These configure fixed-size arrays for deterministic memory usage.

## Building and Testing

The core queue system is platform-independent and can be built and tested standalone without Zephyr:

### Build

```bash
cmake -B build
cmake --build build
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

Or use the convenience target:

```bash
cmake --build build --target check
```

### Generate Coverage Report

Use the automated script:

```bash
./scripts/generate_coverage.sh
```

This will:
1. Build with coverage instrumentation enabled
2. Run all tests
3. Generate HTML coverage report
4. Display coverage summary
5. Open report in browser

Or manually:

```bash
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build
cd build && ctest --output-on-failure
lcov --capture --directory . --output-file coverage.info --ignore-errors mismatch
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/googletest/*' '*/build/_deps/*' '*/samples/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html
```

**Coverage on GitHub**: 
- Coverage reports are automatically generated on every push
- PR comments show coverage changes
- Coverage badge updates on main branch
- HTML reports available as workflow artifacts

## Use Cases

- **Automotive**: Engine monitoring, brake systems, transmission control, body control modules
- **Industrial**: PLCs, motor controllers, process monitoring, SCADA integration
- **Aerospace**: Flight control computers, redundant sensor fusion, avionics
- **Medical**: Patient monitors, infusion pumps, diagnostic equipment
- **Robotics**: Multi-sensor fusion, motor control, safety monitoring

## Performance

- **Engine step**: O(n) where n = number of signals + merges + cyclic outputs
- **Voting algorithms**: O(n log n) for median, O(n) for average/min/max
- **Memory**: 100% static allocation, zero runtime malloc
- **Latency**: Deterministic processing time, configurable cycle period (1-100ms typical)
- **Throughput**: Tested up to 10kHz ISR input rate, 1kHz engine processing rate

## Safety Features

- **Deterministic**: Fixed execution time, no dynamic allocation
- **Thread-safe**: ISR-safe hardware ringbuffer
- **Pure processing**: Engine step has no RTOS dependencies
- **Status tracking**: Every value has quality indicator
- **Redundancy**: Built-in voting for multiple sensors
- **Timeout detection**: Automatic staleness flagging
- **Consistency checking**: Tolerance-based disagreement detection
- **Testable**: Pure functions enable comprehensive unit testing

## Platform Support

**Operating Systems:**
- Native (Linux, macOS, Windows) - for development and testing
- Zephyr RTOS - production embedded systems
- FreeRTOS - via platform adapters

**Toolchains:**
- GCC (Linux, embedded ARM)
- Clang (macOS, embedded)
- IAR Embedded Workbench
- ARM Compiler (ARMCC)
- MSVC (Windows, testing only)

See [docs/platform-portability.md](docs/platform-portability.md) for toolchain details.

## Future Enhancements

- [ ] Additional input sources (I2C, UART, Modbus RTU)
- [ ] Digital filter nodes (moving average, median filter, Kalman)
- [ ] On-change outputs with hysteresis
- [ ] Runtime configuration via UDS/XCP
- [ ] Diagnostic freeze-frame capture
- [ ] Flash-based configuration storage

## License

Apache-2.0

## Contributing

Contributions welcome! Please ensure:
- Code follows existing architecture patterns
- All tests pass (`./all_tests`)
- Coverage maintained or improved
- Documentation updated
- Device tree examples provided for new features
