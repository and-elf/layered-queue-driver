# Zephyr Device Drivers Restoration

## Summary

This directory contains Zephyr device drivers that were accidentally removed during cleanup. These drivers have been restored to enable the base case: **manual device tree configuration working with west alone**, without requiring the Python code generator.

## Architecture

The layered-queue-driver now supports two integration modes:

### Mode 1: Pure Zephyr (Base Case) ✅
- Write device tree overlays manually
- Drivers auto-instantiate using `DT_INST_FOREACH_STATUS_OKAY`
- Build with west (standard Zephyr build)
- **No code generation required**

### Mode 2: Requirements-Driven (Optional)
- Use `scripts/reqgen.py` to translate requirements → device tree nodes
- CMake calls west to build
- Code generation is for DTS creation only, not driver instantiation

## Restored Drivers

### Hardware Input Drivers
- `lq_hw_adc.c` - ADC input (`lq,hw-adc-input`)
- `lq_hw_spi.c` - SPI input (`lq,hw-spi-input`)
- `lq_hw_sensor.c` - Sensor API integration (`lq,hw-sensor-input`)

### Signal Processing Drivers
- `lq_scale.c` - Linear scaling (`lq,scale`)
- `lq_remap.c` - Range remapping (`lq,remap`)
- `lq_pid.c` - PID controller (`lq,pid`)
- `lq_mid_merge.c` - Redundancy voting (`lq,mid-merge`)

### Monitoring & Output Drivers
- `lq_fault_monitor.c` - Threshold monitoring (`lq,fault-monitor`)
- `lq_verified_output.c` - Dual-channel safety (`lq,verified-output`)
- `lq_cyclic_output.c` - Periodic output (`lq,cyclic-output`)
- `lq_gpio_pattern.c` - GPIO pattern generation (`lq,gpio-pattern`)

### Protocol Drivers
- `lq_protocol_j1939.c` - SAE J1939 (`lq,protocol-j1939`)
- Additional protocol drivers can be added similarly

## Usage Example

### 1. Device Tree Overlay (`app.overlay`)

```dts
/ {
    temp_adc: lq-hw-adc {
        compatible = "lq,hw-adc-input";
        io-channels = <&adc0 2>;
        status = "okay";
    };

    temp_scaled: lq-scale {
        compatible = "lq,scale";
        source = <&temp_adc>;
        scale-factor = <100>;
        offset = <-27315>;
        status = "okay";
    };

    temp_fault: lq-fault {
        compatible = "lq,fault-monitor";
        input = <&temp_scaled>;
        threshold-high = <8500>;
        hysteresis = <500>;
        status = "okay";
    };
};
```

### 2. Kconfig (`prj.conf`)

```ini
CONFIG_LQ_DRIVER=y
CONFIG_LQ_HW_ADC=y
CONFIG_LQ_SCALE=y
CONFIG_LQ_FAULT_MONITOR=y
```

### 3. Application Code

```c
#include <zephyr/kernel.h>
#include "lq_engine.h"

void main(void) {
    // Drivers are auto-instantiated by Zephyr
    // Just start the engine
    lq_engine_init();
    
    while (1) {
        lq_engine_process();
        k_msleep(10);
    }
}
```

### 4. Build with West

```bash
west build -b your_board
west flash
```

## Kconfig Options

All drivers now have individual configuration options:

- `CONFIG_LQ_HW_ADC` - Enable ADC input driver
- `CONFIG_LQ_HW_SPI` - Enable SPI input driver
- `CONFIG_LQ_SCALE` - Enable scale driver
- `CONFIG_LQ_PID` - Enable PID controller
- `CONFIG_LQ_FAULT_MONITOR` - Enable fault monitoring
- `CONFIG_LQ_PROTOCOL_J1939` - Enable J1939 protocol
- etc.

Each driver also has a corresponding `*_INIT_PRIORITY` option for initialization ordering.

## Signal ID Assignment

Zephyr drivers use `DT_DEP_ORD()` to automatically assign signal IDs based on device tree dependency order:

```c
.output_signal = DT_DEP_ORD(DT_DRV_INST(inst))
```

This ensures:
- Signal IDs are unique
- Dependencies are initialized before consumers
- No manual ID coordination needed

## Phandle Resolution

Drivers resolve phandle references to signal IDs:

```c
#define LQ_SCALE_SOURCE_SIGNAL(inst) \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, source), \
        (DT_DEP_ORD(DT_INST_PHANDLE(inst, source))), \
        (DT_INST_PROP_OR(inst, input_signal_id, 0)))
```

This supports both:
- v2.0 phandle API: `source = <&temp_adc>;`
- v1.x compatibility: `input-signal-id = <0>;`

## Adding New Drivers

To add a new Zephyr driver:

1. Create `zephyr/drivers/lq_your_driver.c`
2. Define `DT_DRV_COMPAT` matching the binding
3. Implement init function
4. Create instantiation macro with `DT_INST_FOREACH_STATUS_OKAY`
5. Add Kconfig option in `zephyr/Kconfig`
6. Add to `zephyr/CMakeLists.txt` with `zephyr_library_sources_ifdef`

See existing drivers as templates.

## Migration Notes

### Before (Code Generator Required)
```bash
python3 scripts/dts_gen.py app.dts src/
cmake -B build
make -C build
```

### After (Pure West)
```bash
west build -b board
```

The code generator (`dts_gen.py`) is now **optional** and used only for:
- Generating DTS from requirements specifications
- Memory optimization calculations
- Platform-specific ISR generation (non-Zephyr targets)

## Testing

To verify drivers are instantiated:

```bash
west build -b native_posix -- -DCONFIG_LQ_DRIVER=y
# Check build log for "Initialized X driver" messages
```

## Related Files

- [zephyr/CMakeLists.txt](../CMakeLists.txt) - Driver compilation
- [zephyr/Kconfig](../Kconfig) - Configuration options
- [dts/bindings/layered-queue/](../../dts/bindings/layered-queue/) - Device tree schemas
- [zephyr/module.yml](../module.yml) - Zephyr module definition

## References

- [Zephyr Device Driver Model](https://docs.zephyrproject.org/latest/kernel/drivers/index.html)
- [Device Tree Bindings](https://docs.zephyrproject.org/latest/build/dts/bindings.html)
- [LQ DTS Quick Reference](../../docs/dts-quick-reference.md)
