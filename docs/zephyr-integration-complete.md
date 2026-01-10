# Zephyr Integration - Complete Guide

## TL;DR - Does It "Just Work"?

**Yes!** Once you:
1. Add module to `west.yml`
2. Enable `CONFIG_LQ_DRIVER=y` in `prj.conf`
3. Write a `.overlay` file with your DTS configuration
4. Call `lq_engine_init()` and `lq_engine_process()` in your main

Everything else is automatic. The devicetree defines your entire system.

## What Makes This Work

### 1. Zephyr Module Discovery (`zephyr/module.yml`)

```yaml
name: layered-queue
build:
  cmake: .           # Root CMakeLists.txt
  kconfig: Kconfig   # Configuration options
  settings:
    dts_root: .      # Our dts/bindings/ directory
```

When you add this to your west manifest, Zephyr's build system:
- Automatically finds `dts/bindings/layered-queue/*.yaml` bindings
- Includes our `Kconfig` in menuconfig
- Runs our `CMakeLists.txt` as part of the build

**No manual setup needed** - west handles module discovery.

### 2. Device Tree Bindings Schema (`dts/bindings/`)

We provide 21 YAML binding files that define the schema for DTS nodes:

```
dts/bindings/layered-queue/
├── lq,hw-adc-input.yaml       # ADC hardware input
├── lq,hw-gpio-input.yaml      # GPIO hardware input  
├── lq,hw-can-input.yaml       # CAN hardware input
├── lq-scale.yaml              # Scale/transform driver
├── lq,mid-merge.yaml          # Multi-input voting
├── lq-fault-monitor.yaml      # Fault detection
├── lq-pid.yaml                # PID controller
├── lq-verified-output.yaml    # Safety-verified output
├── lq,cyclic-output.yaml      # Periodic transmission
└── ... (12 more bindings)
```

Each binding supports **both v1.x and v2.0 APIs**:

**v1.x (deprecated but functional):**
```dts
temp_scaled: lq-scale {
    compatible = "lq-scale";
    input-signal-id = <0>;      // Manual ID assignment
    output-signal-id = <1>;
    scale-factor = <100>;
};
```

**v2.0 (preferred):**
```dts
temp_scaled: lq-scale {
    compatible = "lq-scale";
    source = <&temp_sensor>;    // Phandle - auto ID!
    scale-factor = <100>;
};
```

Zephyr validates these at compile time:
- Type checking (phandle, int, string, array, etc.)
- Required property enforcement
- Phandle target validation
- Enum value checking

### 3. Zephyr-Style Hardware Bindings

We use standard Zephyr devicetree patterns for hardware:

**ADC (io-channels):**
```dts
temp_sensor: lq-hw-adc {
    compatible = "lq,hw-adc-input";
    io-channels = <&adc0 2>;  // Standard Zephyr ADC binding
};
```

**GPIO (gpios):**
```dts
door_switch: lq-hw-gpio {
    compatible = "lq,hw-gpio-input";
    gpios = <&gpio0 15 GPIO_ACTIVE_HIGH>;  // Standard GPIO binding
};
```

**CAN (can-controller):**
```dts
engine_speed: lq-hw-can {
    compatible = "lq,hw-can-input";
    can-controller = <&can0>;  // Standard CAN binding
    can-id = <0x0CF00400>;
};
```

**I2C Sensors (sensor-device):**
```dts
imu_data: lq-sensor {
    compatible = "lq,sensor-input";
    sensor-device = <&bmi270>;  // Standard sensor binding
};
```

This means:
- Hardware phandles resolve through Zephyr's devicetree
- No custom hardware abstraction needed
- Works with any Zephyr board/SoC
- Portable across platforms

### 4. Code Generation (`scripts/dts_gen.py`)

The build process runs our Python generator:

```cmake
# CMakeLists.txt
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Generate configuration from devicetree
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/lq_config_generated.c
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/dts_gen.py
            --dts ${APPLICATION_BINARY_DIR}/zephyr/zephyr.dts
            --output ${CMAKE_BINARY_DIR}/lq_config_generated.c
    DEPENDS ${APPLICATION_SOURCE_DIR}/app.overlay
    VERBATIM
)
```

The generator:
1. **Reads** Zephyr's devicetree output (`zephyr.dts`)
2. **Resolves** all phandle references to actual nodes
3. **Assigns** signal IDs in dependency order (sources before consumers)
4. **Generates** C initialization code

Example generated code:

```c
// From your DTS overlay
static const lq_hw_adc_config_t hw_adc_configs[] = {
    {
        .output_signal_id = 0,  // temp_sensor
        .adc_device = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(DT_NODELABEL(temp_sensor))),
        .channel = DT_IO_CHANNELS_INPUT(DT_NODELABEL(temp_sensor)),
        .min_value = 0,
        .max_value = 4095,
    },
};

static const lq_scale_config_t scale_configs[] = {
    {
        .input_signal_id = 0,   // temp_sensor (auto-assigned)
        .output_signal_id = 1,  // temp_scaled (auto-assigned)
        .scale_factor = 100,
        .offset = -27315,
    },
};

void lq_engine_init(void) {
    lq_register_hw_adc(hw_adc_configs, ARRAY_SIZE(hw_adc_configs));
    lq_register_scale(scale_configs, ARRAY_SIZE(scale_configs));
}
```

### 5. Build Integration

The complete flow:

```
Developer writes DTS overlay (app.overlay)
           ↓
Zephyr validates against bindings (dts/bindings/*.yaml)
           ↓
Zephyr generates devicetree macros (zephyr/zephyr.dts)
           ↓
Our generator creates init code (lq_config_generated.c)
           ↓
CMake compiles driver + generated code
           ↓
Linker produces firmware
```

## Migration from v1.x to v2.0

### What Changed?

**v1.x Problems:**
- Manual signal ID assignment (error-prone)
- No dependency tracking
- Hard to refactor (renumber everything)
- No type safety for signal connections

**v2.0 Solutions:**
- Phandle-based signal routing (auto IDs)
- Dependency-aware code generation
- Refactor-safe (just change phandles)
- Type-checked connections

### Backward Compatibility

All bindings support both APIs:

```yaml
# lq-scale.yaml
properties:
  source:           # v2.0 - PREFERRED
    type: phandle
    description: Input source node phandle
    
  input-signal-id:  # v1.x - DEPRECATED
    type: int
    required: false  # Made optional for v2.0
    description: Use 'source' phandle instead
```

Old DTS files still work (with deprecation warnings).

### Migration Path

1. **Update one node at a time** - no big-bang required
2. **Test incrementally** - old and new API coexist
3. **Remove manual IDs** when all nodes migrated
4. **Enjoy 20-30% reduction** in DTS file size

## Complete Example

**west.yml:**
```yaml
manifest:
  projects:
    - name: layered-queue-driver
      url: https://github.com/your-org/layered-queue-driver
      path: modules/layered-queue
```

**prj.conf:**
```ini
CONFIG_LQ_DRIVER=y
CONFIG_LQ_MAX_SIGNALS=64
CONFIG_ADC=y
CONFIG_CAN=y
```

**app.overlay:**
```dts
&adc0 {
    status = "okay";
};

&can0 {
    status = "okay";
};

/ {
    temp_raw: lq-hw-adc {
        compatible = "lq,hw-adc-input";
        label = "TEMP_RAW";
        io-channels = <&adc0 2>;
    };
    
    temp_scaled: lq-scale {
        compatible = "lq-scale";
        label = "TEMP_CELSIUS";
        source = <&temp_raw>;
        scale-factor = <100>;
        offset = <-27315>;
    };
    
    temp_tx: lq-can-output {
        compatible = "lq,cyclic-output";
        label = "TEMP_CAN_TX";
        source = <&temp_scaled>;
        can-controller = <&can0>;
        can-id = <0x123>;
        period-ms = <100>;
    };
};
```

**main.c:**
```c
#include <zephyr/kernel.h>
#include <lq_engine.h>

void main(void) {
    lq_engine_init();  // Generated from devicetree
    
    while (1) {
        lq_engine_process();  // Run the pipeline
        k_msleep(10);
    }
}
```

**Build:**
```bash
west build -b your_board -p
west flash
```

**That's it!** No manual driver initialization, no signal ID coordination, no configuration code.

## Troubleshooting

### "Unknown compatible 'lq,scale'"

**Cause:** DTS bindings not found by Zephyr

**Fix:** Check `zephyr/module.yml` has:
```yaml
build:
  settings:
    dts_root: .
```

And that `dts/bindings/layered-queue/lq-scale.yaml` exists.

### "Phandle target 'temp_sensor' not found"

**Cause:** Referenced node doesn't exist or is misspelled

**Fix:** Check node label matches:
```dts
temp_sensor: lq-hw-adc { ... };  // Define

source = <&temp_sensor>;  // Reference (must match)
```

### "Signal dependency cycle detected"

**Cause:** Circular phandle references (A→B→A)

**Fix:** Review signal flow - pipelines must be acyclic:
```dts
// GOOD: Linear flow
A → B → C

// BAD: Cycle
A → B → A
```

### Generated code has duplicate IDs

**Cause:** Generator bug or manual override conflict

**Fix:** Remove all `*-signal-id` properties (let v2.0 auto-assign):
```dts
// Remove this:
output-signal-id = <5>;

// Generator will assign automatically
```

## Advanced Features

### Multi-Source Voting

```dts
sensor_a: lq-hw-adc { io-channels = <&adc0 0>; };
sensor_b: lq-hw-adc { io-channels = <&adc0 1>; };
sensor_c: lq-hw-adc { io-channels = <&adc0 2>; };

pressure_voted: lq-mid-merge {
    compatible = "lq,mid-merge";
    sources = <&sensor_a &sensor_b &sensor_c>;
    algorithm = "median";
};
```

### Fault Monitoring → Limp-Home

```dts
temp_fault: lq-fault-monitor {
    input = <&temp_sensor>;
    threshold-high = <8500>;
};

safe_output: lq-limp-home {
    normal-input = <&pid_output>;
    fault-signal = <&temp_fault>;
    safe-value = <0>;  // Shutdown on overheat
};
```

### PID Control Loop

```dts
pid_controller: lq-pid {
    setpoint = <&desired_temp>;
    measurement = <&actual_temp>;
    kp = <100>;
    ki = <10>;
    kd = <50>;
};
```

## Documentation

- [DTS Quick Reference](dts-quick-reference.md) - Complete v2.0 API
- [Migration Guide](dts-api-migration.md) - v1.x → v2.0 upgrade
- [Platform Quick Reference](platform-quick-reference.md) - Hardware patterns
- [Testing Guide](testing.md) - HIL and unit test setup

## Summary

**Yes, it "just works"** with Zephyr:

✅ Add to west.yml → Zephyr discovers module  
✅ Write DTS overlay → Bindings validate schema  
✅ Build → Generator creates init code  
✅ Run → Devicetree-configured system executes  

No glue code, no manual initialization, no configuration boilerplate.

**The devicetree IS the configuration.**
