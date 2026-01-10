# DTS API Quick Reference (v2.0)

## Property Naming Convention

All properties use **phandle references** instead of numeric IDs.

### Signal Flow Properties

| Property    | Type             | Used By                          | Example                        |
|-------------|------------------|----------------------------------|--------------------------------|
| `source`    | Single phandle   | Scale, Remap, PID, Outputs       | `source = <&temp_sensor>;`     |
| `sources`   | Phandle array    | Merge (2+ inputs)                | `sources = <&s1 &s2 &s3>;`     |
| `input`     | Single phandle   | Fault monitors, verified outputs | `input = <&scaled_value>;`     |
| `setpoint`  | Single phandle   | PID controllers                  | `setpoint = <&target_temp>;`   |
| `measurement` | Single phandle | PID controllers                  | `measurement = <&actual_rpm>;` |
| `command`   | Single phandle   | Verified outputs                 | `command = <&heater_cmd>;`     |
| `verification` | Single phandle | Verified outputs              | `verification = <&heater_fb>;` |

**Rule:** Signal IDs are auto-assigned in node definition order. No manual `signal-id` needed.

### Hardware Peripheral Properties (Zephyr-Style)

| Property          | Type              | Example                                      |
|-------------------|-------------------|----------------------------------------------|
| `io-channels`     | ADC phandle       | `io-channels = <&adc0 2>;`                   |
| `gpios`           | GPIO phandle+flags| `gpios = <&gpio0 15 GPIO_ACTIVE_HIGH>;`      |
| `spi-device`      | SPI phandle       | `spi-device = <&spi1>;`                      |
| `can-controller`  | CAN phandle       | `can-controller = <&can0>;`                  |
| `sensor-device`   | Sensor phandle    | `sensor-device = <&tmp117_engine>;`          |
| `pwm-phase-a/b/c` | PWM phandles      | `pwm-phase-a = <&pwm0 0>;`                   |

## Complete Example

```dts
/ {
    /* Hardware layer - ADC input */
    temp_adc: lq-adc@0 {
        compatible = "lq,adc-source";
        io-channels = <&adc0 0>;     /* Zephyr-style ADC reference */
        sample-rate-hz = <10>;
        staleness-us = <200000>;
    };
    
    /* Processing layer - Scale to engineering units */
    temp_celsius: lq-scale@0 {
        compatible = "lq,scale";
        source = <&temp_adc>;        /* Phandle reference */
        scale-factor = <37>;         /* 0.037°C per ADC count */
        offset = <0>;
        clamp-min = <0>;
        clamp-max = <150>;
    };
    
    /* Monitoring layer - Fault detection */
    temp_monitor: lq-fault-monitor@0 {
        compatible = "lq,fault-monitor";
        input = <&temp_celsius>;     /* Phandle reference */
        
        check-range;
        max-value = <120>;           /* Fault if > 120°C */
        
        check-staleness;
        stale-timeout-us = <500000>;
        
        fault-level = <2>;
        wake-function = "temp_fault_handler";
    };
    
    /* Output layer - Send via J1939 */
    temp_can_out: lq-cyclic-output@0 {
        compatible = "lq,cyclic-j1939";
        source = <&temp_celsius>;    /* Phandle reference */
        pgn = <0xFEEE00>;
        period-us = <100000>;        /* 10 Hz */
        priority = <6>;
    };
    
    /* Fail-safe GPIO output */
    safe_gpio: lq-gpio-output@0 {
        compatible = "lq,gpio-output";
        source = <&temp_monitor>;    /* Fault status */
        gpios = <&gpio0 26 GPIO_ACTIVE_HIGH>;  /* Zephyr-style */
    };
};
```

## Migration Checklist

Converting from v1.x to v2.0:

- [ ] Remove all `signal-id` properties (auto-assigned now)
- [ ] Change `source-signal` → `source` with phandle
- [ ] Change `input-signal` → `input` with phandle  
- [ ] Change `input-signal-ids` → `sources` with phandle array
- [ ] Remove `output-signal`, `fault-output-signal` (auto-assigned)
- [ ] Convert `adc-channel` + `adc-device` → `io-channels`
- [ ] Convert `gpio-pin` → `gpios` with GPIO flags
- [ ] Convert `can-interface` string → `can-controller` phandle
- [ ] Verify all phandle references point to valid labeled nodes

## Common Patterns

### Dual-Sensor Merging with Voting

```dts
sensor_a: lq-hw-adc@0 {
    io-channels = <&adc0 0>;
};

sensor_b: lq-hw-adc@1 {
    io-channels = <&adc0 1>;
};

merged: lq-merge@0 {
    sources = <&sensor_a &sensor_b>;  /* Array of phandles */
    voting-method = "median";
    tolerance = <50>;
};
```

### PID Control Loop

```dts
setpoint: lq-constant@0 {
    value = <75>;  /* Target: 75°C */
};

actual_temp: lq-scale@0 {
    source = <&temp_adc>;
    /* ... scaling config ... */
};

pid: lq-pid@0 {
    setpoint = <&setpoint>;
    measurement = <&actual_temp>;
    kp = <20000>;
    ki = <500>;
    kd = <5000>;
};
```

### Verified Output with Feedback

```dts
heater_cmd: lq-comparator@0 {
    input = <&pid_output>;
    threshold = <500>;
};

heater_fb: lq-gpio@0 {
    gpios = <&gpio0 15 GPIO_ACTIVE_HIGH>;  /* Feedback pin */
};

verified_heater: lq-verified-output@0 {
    command = <&heater_cmd>;
    verification = <&heater_fb>;
    tolerance = <0>;
    verification-timeout-us = <50000>;
};
```

## Advantages Over v1.x

| Aspect              | v1.x (Manual IDs)       | v2.0 (Phandles)              |
|---------------------|-------------------------|------------------------------|
| Signal tracking     | Manual ID assignment    | Auto-assigned                |
| Refactoring         | Breaks on renumber      | Safe (references auto-update)|
| Type safety         | Runtime errors          | Parse-time validation        |
| Readability         | Need comments for IDs   | Self-documenting             |
| Hardware binding    | Custom properties       | Standard Zephyr bindings     |
| Portability         | Hardcoded peripherals   | Easy retargeting             |
| Boilerplate         | ~30% more lines         | Minimal boilerplate          |

## FAQ

**Q: What if I need to reference a signal by numeric ID?**  
A: You generally don't. Phandles are resolved to auto-assigned IDs at parse time. If you absolutely need numeric IDs for external APIs, they're assigned sequentially in node definition order (0, 1, 2, ...).

**Q: Can I mix v1.x and v2.0 syntax?**  
A: Yes, backward compatibility is maintained. Old property names (`source-signal`, etc.) still work, but new code should use v2.0 syntax.

**Q: How do I find what ID was assigned to a node?**  
A: Check the generated C code, or use `--dump-signals` flag with the DTS parser. But generally you shouldn't need to know - just use phandles.

**Q: What about multi-output nodes?**  
A: Each output property (`source`, `input`, etc.) references exactly one node. For multi-channel outputs, create separate output nodes.

**Q: Can phandles reference nodes in different files?**  
A: Yes! As long as the node has a label, you can reference it from any file that includes it (via `/include/` or overlay mechanism).
