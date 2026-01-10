# Layered Queue Driver for Zephyr

This driver can be used both as:
1. **Standalone library** - Build and test independently with CMake
2. **Zephyr module** - Integrate into Zephyr projects via west

## Standalone Build (Native Testing)

```bash
# Build
cmake -B build
cmake --build build

# Test
cd build && ctest --output-on-failure

# Coverage
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build
cd build && ctest
```

## Zephyr Integration

### As a Zephyr Module

Add to your Zephyr project's `west.yml`:

```yaml
manifest:
  projects:
    - name: layered-queue
      url: https://github.com/your-org/zephyr-declarative-drivers
      revision: main
      path: modules/layered-queue
```

Then in your `prj.conf`:

```ini
CONFIG_LQ_DRIVER=y
CONFIG_LQ_QUEUE=y
CONFIG_LQ_ADC_SOURCE=y
CONFIG_LQ_SPI_SOURCE=y
CONFIG_LQ_MERGE_VOTER=y
```

### Device Tree Example (v2.0 Phandle API)

The driver now uses **phandle-based signal routing** which auto-assigns signal IDs and eliminates manual coordination:

```dts
/ {
    // Hardware: ADC temperature sensor
    temp_raw: lq-hw-adc {
        compatible = "lq,hw-adc-input";
        label = "TEMP_RAW";
        io-channels = <&adc0 2>;  // Zephyr-style hardware binding
    };

    // Scale raw ADC to degrees Celsius
    temp_scaled: lq-scale {
        compatible = "lq-scale";
        label = "TEMP_SCALED";
        source = <&temp_raw>;  // Phandle reference - no manual signal ID!
        
        scale-factor = <100>;  // Scale by 0.01
        offset = <-27315>;     // Offset to convert to °C
    };

    // Fault monitor: trigger if temp > 85°C
    temp_fault: lq-fault {
        compatible = "lq,fault-monitor";
        label = "TEMP_FAULT";
        input = <&temp_scaled>;  // Phandle reference
        
        threshold-high = <8500>;  // 85.00°C
        hysteresis = <500>;       // 5.00°C
    };

    // CAN output: transmit temp every 100ms
    temp_tx: lq-can-output {
        compatible = "lq,cyclic-output";
        label = "TEMP_CAN_TX";
        source = <&temp_scaled>;  // Phandle reference
        
        can-controller = <&can0>;  // Zephyr-style CAN binding
        can-id = <0x123>;
        period-ms = <100>;
    };
};
```

**Key Features:**
- Signal IDs auto-assigned by DTS generator
- Phandles provide type safety and clear dependencies
- Zephyr-style hardware bindings (`io-channels`, `can-controller`, `gpios`)
- Human-readable labels for debugging
- No manual signal ID coordination

See [docs/dts-quick-reference.md](docs/dts-quick-reference.md) for complete v2.0 API reference.

### Application Code

```c
#include <zephyr/kernel.h>
#include <lq_engine.h>

// Devicetree-generated code auto-initializes everything
// Your main just needs to start the engine

void main(void) {
    // Initialize the layered queue engine from devicetree
    lq_engine_init();
    
    // Process inputs, run pipeline, generate outputs
    while (1) {
        lq_engine_process();
        k_msleep(10);  // 100Hz update rate
    }
}
```

**That's it!** The devicetree fully defines:
- Hardware inputs (ADC, GPIO, CAN, SPI, sensors)
- Signal processing pipeline (scale, merge, PID, fault detection)
- Output drivers (CAN, J1939, GPIO, PWM)
- All signal routing via phandles

No manual initialization code needed.

## How Zephyr Integration Works

The layered queue driver integrates seamlessly with Zephyr through three mechanisms:

### 1. **Zephyr Module Definition** (`zephyr/module.yml`)

This file tells Zephyr where to find our code:

```yaml
build:
  cmake: .          # Our CMakeLists.txt
  kconfig: Kconfig  # Our Kconfig options
  settings:
    dts_root: .     # Our dts/bindings/ directory
```

When you add this module to your `west.yml` manifest, Zephyr automatically:
- Discovers our Kconfig options (shows up in `menuconfig`)
- Finds our DTS bindings in `dts/bindings/layered-queue/*.yaml`
- Includes our CMakeLists.txt in the build

**No manual setup required** - west and Zephyr's build system handle everything.

### 2. **Device Tree Bindings** (`dts/bindings/layered-queue/*.yaml`)

These YAML files define the schema for our DTS nodes. Zephyr's devicetree compiler:
- Validates your `.dts` overlay files against the schema
- Generates C macros in `devicetree_generated.h`
- Provides type-checked accessors (`DT_PROP`, `DT_PHANDLE`, etc.)

Example binding (`lq-scale.yaml`):

```yaml
compatible: "lq-scale"

properties:
  source:
    type: phandle        # Zephyr validates this is a node reference
    description: Input source node phandle
  
  scale-factor:
    type: int
    required: true
    description: Scaling multiplier (fixed-point)
```

When you write in your DTS:

```dts
temp_scaled: lq-scale {
    compatible = "lq-scale";
    source = <&temp_raw>;  // Zephyr validates temp_raw exists
    scale-factor = <100>;
};
```

Zephyr generates:

```c
#define DT_N_temp_scaled_P_source  DT_N_temp_raw  // Phandle resolved!
#define DT_N_temp_scaled_P_scale_factor  100
```

### 3. **DTS Code Generator** (`scripts/dts_gen.py`)

Our Python script runs during the build and:
- Parses Zephyr's generated devicetree macros
- Resolves phandle references to actual nodes
- Auto-assigns signal IDs in dependency order
- Generates C initialization code (`lq_config_generated.c`)

**Build integration:**

```cmake
# CMakeLists.txt automatically runs generator
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/lq_config_generated.c
    COMMAND python3 ${PROJECT_SOURCE_DIR}/scripts/dts_gen.py
            --dts ${DTS_ROOT}/app.dts
            --output ${CMAKE_BINARY_DIR}/lq_config_generated.c
    DEPENDS ${DTS_ROOT}/app.dts
)
```

The generated code creates the full driver configuration:

```c
// Generated from your devicetree
static const lq_scale_config_t scale_configs[] = {
    {
        .input_signal_id = 0,   // temp_raw (auto-assigned)
        .output_signal_id = 1,  // temp_scaled (auto-assigned)
        .scale_factor = 100,
        .offset = -27315,
    },
    // ... more drivers
};

void lq_engine_init(void) {
    // Register all drivers from devicetree
    lq_register_scale(scale_configs, ARRAY_SIZE(scale_configs));
    // ... etc
}
```

### Putting It All Together

1. **Developer writes DTS** (defines system in `.overlay` file)
2. **Zephyr validates** (bindings check schema, types, phandles)
3. **Zephyr generates macros** (`devicetree_generated.h`)
4. **Our generator creates init code** (`lq_config_generated.c`)
5. **CMake compiles** (driver + generated code → firmware)
6. **Application calls `lq_engine_init()`** - everything just works!

**Migration from v1.x:**
- Old: Manual signal ID assignment, error-prone coordination
- v2.0: Phandles auto-resolve, generator assigns IDs in dependency order
- All bindings support both APIs (v1.x deprecated but functional)

## Architecture

```
┌─────────────────────────────────────┐
│   Application Code                   │
│   (Native or Zephyr)                │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│   Platform-Independent Core         │
│   - lq_queue_core.c                 │
│   - layered_queue_core.h            │
└────────────┬────────────────────────┘
             │
     ┌───────┴────────┐
     │                │
┌────▼─────┐   ┌─────▼────┐
│  Native  │   │  Zephyr  │
│ Platform │   │ Platform │
│ (POSIX)  │   │ (Kernel) │
└──────────┘   └──────────┘
```

## Directory Structure

```
.
├── CMakeLists.txt           # Standalone CMake build
├── include/
│   ├── layered_queue_core.h # Platform-independent API
│   ├── lq_platform.h        # Platform abstraction
│   └── zephyr/drivers/
│       ├── layered_queue.h  # Zephyr device API
│       └── layered_queue_internal.h
├── src/
│   ├── lq_queue_core.c      # Core implementation
│   └── platform/
│       ├── lq_platform_native.c   # POSIX impl
│       └── lq_platform_zephyr.c   # Zephyr impl
├── drivers/                 # Zephyr module
│   ├── CMakeLists.txt
│   └── layered_queue/
│       ├── Kconfig
│       ├── lq_queue.c       # DT wrapper
│       └── lq_*.c           # Source drivers
├── tests/
│   └── queue_test.cpp       # Google Test suite
├── dts/
│   ├── bindings/            # DT bindings
│   └── examples/            # Example overlays
└── zephyr/
    └── module.yml           # Zephyr module definition
```

## Features

- ✅ Platform-independent core
- ✅ Full test coverage with GoogleTest
- ✅ Zephyr device tree integration
- ✅ CI/CD ready (native + Zephyr builds)
- ✅ No dynamic allocation
- ✅ Thread-safe operations
- ✅ Configurable drop policies
- ✅ Statistics tracking

## License

Apache-2.0
