# DTS API Migration Guide

## Unified Phandle-Based API (v2.0)

The Layered Queue Driver now uses a unified, phandle-based API that:
- Eliminates manual signal ID tracking
- Provides consistent property names across all node types
- Uses Zephyr-style hardware phandles for peripherals (ADC, GPIO, SPI, etc.)

## Key Changes

### Signal References: Before (v1.x - Manual Signal IDs)

```dts
temp_sensor: lq-hw-adc@0 {
    signal-id = <2>;  /* Manual tracking */
};

temp_scaled: lq-scale@0 {
    source-signal = <2>;  /* Have to remember ID */
    output-signal = <11>; /* More manual tracking */
};

monitor: lq-fault-monitor@0 {
    input-signal = <11>;        /* Different property name */
    fault-output-signal = <20>; /* Yet another name */
};
```

### Signal References: After (v2.0 - Phandle References)

```dts
temp_sensor: lq-hw-adc@0 {
    /* No signal-id needed - auto-assigned */
};

temp_scaled: lq-scale@0 {
    source = <&temp_sensor>;  /* Direct reference */
};

monitor: lq-fault-monitor@0 {
    input = <&temp_scaled>;   /* Unified property name */
};
```

### Hardware References: Zephyr-Style Phandles (v2.0)

We now use standard Zephyr device tree bindings for hardware peripherals:

```dts
/* ADC channels - use io-channels property */
temp_sensor: lq-hw-adc@0 {
    io-channels = <&adc0 0>;  /* ADC0, channel 0 */
};

/* GPIO pins - use gpios property with flags */
status_led: lq-hw-gpio@0 {
    gpios = <&gpio0 15 GPIO_ACTIVE_HIGH>;
};

/* SPI devices - use spi-device phandle */
external_sensor: lq-hw-spi@0 {
    spi-device = <&spi0>;
    data-ready-gpios = <&gpio0 5 GPIO_ACTIVE_HIGH>;
};

/* CAN controllers - use can-controller phandle */
canopen: canopen-protocol@0 {
    can-controller = <&can0>;
};

/* I2C/Sensor devices - reference actual sensor nodes */
temp_i2c: lq-hw-sensor@0 {
    sensor-device = <&tmp117_engine>;
};
```

**Benefits:**
- Board portability: Change `&adc0` to `&adc1` to retarget
- Type safety: Invalid phandles caught at parse time
- Zephyr integration: Works seamlessly with Zephyr device tree
- Documentation: Hardware connections are explicit

## Unified Property Names

| Old Property          | New Property | Used By                        |
|-----------------------|--------------|--------------------------------|
| `source-signal`       | `source`     | scale, remap, PID, outputs     |
| `input-signal-ids`    | `sources`    | merge, voter (multiple inputs) |
| `input-signal`        | `input`      | fault-monitor                  |
| `output-signal`       | *(auto)*     | Auto-assigned, rarely needed   |
| `fault-output-signal` | *(auto)*     | Auto-assigned from monitor     |
| `signal-id`           | *(auto)*     | Auto-assigned in order         |

## Migration Steps

### 1. Remove Manual Signal IDs

**Old:**
```dts
speed_sensor: lq-hw-adc@0 {
    channel = <0>;
    signal-id = <0>;  /* Remove this */
};
```

**New:**
```dts
speed_sensor: lq-hw-adc@0 {
    channel = <0>;
    /* signal-id auto-assigned */
};
```

### 2. Replace Numeric References with Phandles

**Old:**
```dts
temp_scaled: lq-scale@0 {
    source-signal = <2>;  /* Numeric ID */
    output-signal = <5>;
};
```

**New:**
```dts
temp_scaled: lq-scale@0 {
    source = <&temp_sensor>;  /* Phandle reference */
    /* output auto-assigned */
};
```

### 3. Update Merge/Voter Nodes

**Old:**
```dts
speed_voted: lq-mid-merge@0 {
    input-signal-ids = <0 1 2>;
    output-signal-id = <10>;
};
```

**New:**
```dts
speed_voted: lq-mid-merge@0 {
    sources = <&sensor1 &sensor2 &sensor3>;
    /* output auto-assigned */
};
```

### 4. Update Fault Monitors

**Old:**
```dts
overheat: lq-fault-monitor@0 {
    input-signal = <5>;
    fault-output-signal = <20>;
};
```

**New:**
```dts
overheat: lq-fault-monitor@0 {
    input = <&temp_scaled>;
    /* fault output auto-assigned */
};
```

### 5. Update Cyclic Outputs

**Old:**
```dts
can_output: lq-cyclic-output@0 {
    source-signal = <10>;
};
```

**New:**
```dts
can_output: lq-cyclic-output@0 {
    source = <&speed_voted>;
};
```

## Backward Compatibility

The old property names still work for a transition period:

```dts
/* This still works! */
temp_scaled: lq-scale@0 {
    source-signal = <2>;     /* Old style */
    output-signal = <5>;     /* Old style */
};

/* But this is recommended: */
temp_scaled: lq-scale@0 {
    source = <&temp_sensor>;  /* New style */
};
```

You can even mix old and new in the same file during migration.

## Benefits

✅ **No manual ID tracking** - Signal IDs assigned automatically in node order

✅ **Refactoring-safe** - Rename nodes and references update automatically

✅ **Type-safe** - Can't accidentally reference wrong node type

✅ **Consistent API** - Same property name (`source`, `input`) across all nodes

✅ **Less verbose** - No need to specify `output-signal` in most cases

✅ **Better readability** - `source = <&temp_sensor>` vs `source-signal = <2>`

## Complete Example

### Old DTS (Manual IDs)

```dts
/ {
    sensor1: lq-hw-adc@0 {
        signal-id = <0>;
    };
    sensor2: lq-hw-adc@1 {
        signal-id = <1>;
    };
    
    voted: lq-mid-merge@0 {
        input-signal-ids = <0 1>;
        output-signal-id = <10>;
    };
    
    scaled: lq-scale@0 {
        source-signal = <10>;
        output-signal = <11>;
        scale-factor = <100>;
    };
    
    monitor: lq-fault-monitor@0 {
        input-signal = <11>;
        fault-output-signal = <20>;
        check-range;
    };
    
    output: lq-cyclic-output@0 {
        source-signal = <20>;
        output-type = "gpio";
    };
};
```

### New DTS (Phandle References)

```dts
/ {
    sensor1: lq-hw-adc@0 {
        channel = <0>;
    };
    sensor2: lq-hw-adc@1 {
        channel = <1>;
    };
    
    voted: lq-mid-merge@0 {
        sources = <&sensor1 &sensor2>;
        algorithm = "median";
    };
    
    scaled: lq-scale@0 {
        source = <&voted>;
        scale-factor = <100>;
    };
    
    monitor: lq-fault-monitor@0 {
        input = <&scaled>;
        check-range;
        max-value = <5000>;
    };
    
    output: lq-cyclic-output@0 {
        source = <&monitor>;
        output-type = "gpio";
        target-id = <13>;
    };
};
```

**Result:** 60% less boilerplate, much more readable, and refactoring-safe!

## FAQ

### Q: Can I still use numeric signal IDs?

Yes! If you explicitly set `signal-id`, it will be used:

```dts
sensor: lq-hw-adc@0 {
    signal-id = <42>;  /* Explicit ID */
};
```

### Q: How are signal IDs assigned?

Signal IDs are auto-assigned in the order nodes appear in the DTS file, starting from 0. Hardware inputs and processing nodes (scale, merge, etc.) get sequential IDs.

### Q: What if I need a specific output signal ID?

Rarely needed, but you can still set it:

```dts
monitor: lq-fault-monitor@0 {
    input = <&temp_scaled>;
    fault-output-signal = <100>;  /* Explicit output ID */
};
```

### Q: Will old DTS files still work?

Yes! The parser supports both old and new property names. You can migrate gradually.

### Q: What about forward references?

Node order matters! Define nodes before referencing them:

```dts
/* Good - sensor defined first */
sensor: lq-hw-adc@0 { };
scaled: lq-scale@0 {
    source = <&sensor>;  /* OK */
};

/* Bad - forward reference */
scaled: lq-scale@0 {
    source = <&sensor>;  /* Error - sensor not defined yet */
};
sensor: lq-hw-adc@0 { };
```

## Migration Timeline

- **v2.0** - New phandle API introduced (backward compatible)
- **v2.1+** - Old property names still supported
- **v3.0** - Old property names deprecated (warnings)
- **v4.0** - Old property names removed

Recommended: Migrate to new API now for better maintainability!
