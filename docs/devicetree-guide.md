# Layered Queue Driver - Device Tree Guide

## Introduction

This guide explains how to configure the Layered Queue Driver using Zephyr device tree syntax. The driver supports **complete auto-generation** from DTS specifications, eliminating manual struct initialization and ISR boilerplate.

## Two Approaches

### 1. Legacy Approach (Queue-based)
- Manual queue configuration
- Explicit source/voter/output nodes
- More C boilerplate required

### 2. **New Layered Architecture (Auto-Generated)** ⭐
- Complete system defined in DTS
- Auto-generated engine struct initialization
- Auto-generated ISR handlers
- **Recommended for new projects**

---

## Auto-Generated Architecture

### Overview

The new architecture enables complete code generation:

```c
/* ALL of this is auto-generated from DTS: */
static struct lq_engine engine = LQ_ENGINE_DT_INIT(DT_NODELABEL(engine));
LQ_FOREACH_HW_ADC_INPUT(LQ_GEN_ADC_ISR_HANDLER)
LQ_FOREACH_HW_SPI_INPUT(LQ_GEN_SPI_ISR_HANDLER)
```

### Quick Start

**1. Include the header in your C file:**
```c
#include "lq_devicetree.h"
```

**2. Define your system in DTS:**

See `samples/automotive/app.dts` for complete example.

**3. Use the generated code:**
```c
int main(void) {
    lq_hw_input_init();
    lq_engine_start(&engine);  /* Auto-configured from DTS */
}
```

### DTS Node Types

#### Engine Root Node

```dts
engine: lq-engine {
    compatible = "lq,engine";
    max-signals = <32>;
    max-merges = <8>;
    max-cyclic-outputs = <16>;
};
```

#### Hardware ADC Input (ISR Auto-Generated)

```dts
rpm_sensor: lq-hw-adc-input@0 {
    compatible = "lq,hw-adc-input";
    status = "okay";
    
    signal-id = <0>;          /* Where to store in engine.signals[] */
    adc-channel = <0>;
    adc-device = <&adc0>;
    isr-priority = <3>;
    stale-us = <5000>;        /* 5ms staleness timeout */
};
```

**Generated ISR automatically:**
- Reads ADC value on interrupt
- Calls `lq_hw_push(signal_id, value, timestamp)`
- Wakes engine task

#### Hardware SPI Input (ISR Auto-Generated)

```dts
rpm_secondary: lq-hw-spi-input@0 {
    compatible = "lq,hw-spi-input";
    status = "okay";
    
    signal-id = <1>;
    spi-device = <&spi0>;
    data-ready-gpio = <&gpio0 5 0>;
    isr-priority = <3>;
    num-bytes = <2>;
    signed;
    stale-us = <5000>;
    init-priority = <80>;
};
```

**Generated ISR automatically:**
- Triggered by GPIO data-ready interrupt
- Reads SPI sensor
- Calls `lq_hw_push(signal_id, value, timestamp)`

#### Hardware Sensor Input (Zephyr Sensor API) ⭐

**Preferred for real sensors with Zephyr drivers:**

```dts
/* Reference actual sensor device */
temp_sensor: lq-hw-sensor-input@0 {
    compatible = "lq,hw-sensor-input";
    status = "okay";
    
    signal-id = <2>;
    sensor-device = <&tmp117_engine>;  /* Phandle to sensor */
    sensor-channel = <0>;              /* SENSOR_CHAN_AMBIENT_TEMP */
    trigger-type = "data-ready";       /* or "poll", "threshold" */
    scale-factor = <100>;              /* Convert to int32 */
    offset = <0>;
    stale-us = <100000>;
    init-priority = <80>;              /* After sensor driver (70) */
};

/* Actual sensor driver node (in board DTS) */
&i2c0 {
    tmp117_engine: tmp117@48 {
        compatible = "ti,tmp117";
        reg = <0x48>;
        status = "okay";
    };
};
```

**Supported sensors:** TMP117, BME280, LSM6DSL, ICP10125, LPS22HH, MPU6050, and any Zephyr sensor driver!

**Generated code automatically:**
- Configures sensor trigger (data-ready interrupt or polling)
- Fetches via `sensor_sample_fetch()` / `sensor_channel_get()`
- Scales to int32_t and calls `lq_hw_push()`

**Init priority ensures sensors are ready:**
- 50-59: I2C/SPI bus drivers
- 60-69: Sensor drivers
- 80: Hardware input layer (ISR handlers)
- 85: Engine layer

#### Mid-Level Merge (Voter)

```dts
rpm_merged: lq-mid-merge@0 {
    compatible = "lq,mid-merge";
    status = "okay";
    
    output-signal-id = <10>;        /* Result stored here */
    input-signal-ids = <0 1>;       /* Merge signals 0 and 1 */
    voting-method = "median";       /* or "average", "min", "max" */
    tolerance = <50>;               /* Max allowed spread */
    stale-us = <10000>;
};
```

**Voting methods:**
- `"median"` - Best for 3+ sensors, rejects outliers
- `"average"` - Smooth but sensitive to outliers
- `"min"` - Conservative (use lowest value)
- `"max"` - Aggressive (use highest value)

#### Cyclic Output (Deadline Scheduling)

```dts
can_rpm: lq-cyclic-output@0 {
    compatible = "lq,cyclic-output";
    status = "okay";
    
    source-signal-id = <10>;        /* Read from this signal */
    output-type = "j1939";          /* or "can", "canopen", "gpio", "uart" */
    target-id = <0xFEF1>;           /* J1939 PGN or CAN ID */
    period-us = <100000>;           /* 100ms = 10Hz */
    priority = <3>;                 /* CAN priority (0=highest) */
    deadline-offset-us = <0>;       /* Stagger outputs */
};
```

**Auto-generated deadline scheduling** prevents drift.

### Complete Example

```dts
/dts-v1/;

/ {
    engine: lq-engine {
        compatible = "lq,engine";
    };
    
    /* Hardware inputs - ISRs auto-generated */
    rpm_adc: lq-hw-adc-input@0 {
        compatible = "lq,hw-adc-input";
        status = "okay";
        signal-id = <0>;
        adc-channel = <0>;
        adc-device = <&adc0>;
        stale-us = <5000>;
    };
    
    rpm_spi: lq-hw-spi-input@0 {
        compatible = "lq,hw-spi-input";
        status = "okay";
        signal-id = <1>;
        spi-device = <&spi0>;
        data-ready-gpio = <&gpio0 5 0>;
        num-bytes = <2>;
        signed;
        stale-us = <5000>;
    };
    
    /* Mid-level merge - auto-configured */
    rpm_merge: lq-mid-merge@0 {
        compatible = "lq,mid-merge";
        status = "okay";
        output-signal-id = <10>;
        input-signal-ids = <0 1>;
        voting-method = "median";
        tolerance = <50>;
    };
    
    /* Cyclic output - auto-scheduled */
    can_rpm: lq-cyclic-output@0 {
        compatible = "lq,cyclic-output";
        status = "okay";
        source-signal-id = <10>;
        output-type = "j1939";
        target-id = <0xFEF1>;
        period-us = <100000>;
        priority = <3>;
    };
};
```

**Generated code:**
```c
#include "lq_devicetree.h"

/* Auto-generated from DTS */
static struct lq_engine engine = LQ_ENGINE_DT_INIT(DT_NODELABEL(engine));
LQ_FOREACH_HW_ADC_INPUT(LQ_GEN_ADC_ISR_HANDLER)
LQ_FOREACH_HW_SPI_INPUT(LQ_GEN_SPI_ISR_HANDLER)

int main(void) {
    lq_hw_input_init();
    lq_engine_start(&engine);
    return 0;
}
```

### Signal ID Mapping

Signal IDs are your choice - use any scheme:

```dts
/* Option 1: Sequential */
rpm_adc:  signal-id = <0>;
rpm_spi:  signal-id = <1>;
temp_adc: signal-id = <2>;
rpm_merge: output-signal-id = <10>;  /* Results start at 10 */

/* Option 2: Semantic ranges */
/* 0-9: Raw ADC inputs */
/* 10-19: Raw SPI inputs */  
/* 100-199: Merged/validated results */
/* 200-299: Calculated values */
```

### Benefits

1. **No manual struct initialization** - all from DTS
2. **Auto-generated ISR handlers** - just define inputs
3. **Compile-time signal IDs** - no magic numbers
4. **Easy to add sensors** - add DTS node, rebuild
5. **Declarative configuration** - system visible in one file
6. **Type-safe** - devicetree validation catches errors

### See Also

- `samples/automotive/` - Complete auto-generated example
- `zephyr/include/lq_devicetree.h` - Macro definitions
- `docs/architecture.md` - Layered architecture details

---

## Legacy Queue-Based Approach

### Quick Reference

### Include the Header

Add to your `.dts` file:

```dts
#include <zephyr/dt-bindings/layered_queue.h>
```

### Basic Queue

```dts
my_queue: lq-queue@0 {
    compatible = "zephyr,lq-queue";
    capacity = <16>;
    drop-policy = "drop-oldest";
    priority = <10>;
};
```

### ADC Source with Ranges

```dts
adc_temp: lq-source@0 {
    compatible = "zephyr,lq-adc-source";
    adc = <&adc1>;
    channel = <3>;
    output-queue = <&my_queue>;
    poll-interval-ms = <100>;
    averaging = <4>;

    /* Define ranges from most specific to most general */
    normal {
        min = <2000>;
        max = <2500>;
        status = <LQ_OK>;
    };

    warning {
        min = <1800>;
        max = <2700>;
        status = <LQ_DEGRADED>;
    };

    fault {
        min = <0>;
        max = <4095>;
        status = <LQ_OUT_OF_RANGE>;
    };
};
```

### SPI Source with Expected Values

```dts
spi_sensor: lq-source@1 {
    compatible = "zephyr,lq-spi-source";
    spi = <&spi2>;
    reg = <0x20>;
    output-queue = <&my_queue>;
    poll-interval-ms = <50>;
    read-length = <2>;

    ok {
        value = <0xAA55>;
        status = <LQ_OK>;
    };

    error {
        value = <0xFFFF>;
        status = <LQ_ERROR>;
    };
};
```

### Merge/Voter

```dts
q_sensor1: lq-queue@1 { ... };
q_sensor2: lq-queue@2 { ... };
q_output: lq-queue@3 { ... };

voter: lq-merge@0 {
    compatible = "zephyr,lq-merge-voter";
    input-queues = <&q_sensor1 &q_sensor2>;
    output-queue = <&q_output>;
    voting-method = "median";
    tolerance = <100>;
    status-if-violation = <LQ_INCONSISTENT>;
    timeout-ms = <500>;

    valid_range {
        min = <1000>;
        max = <3000>;
        status-if-violation = <LQ_OUT_OF_RANGE>;
    };
};
```

## Property Reference

### Queue Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `compatible` | string | Yes | - | Must be `"zephyr,lq-queue"` |
| `capacity` | int | Yes | - | Maximum number of items |
| `drop-policy` | string | No | `"drop-oldest"` | `"drop-oldest"`, `"drop-newest"`, or `"block"` |
| `priority` | int | No | `0` | Scheduling priority |

### ADC Source Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `compatible` | string | Yes | - | Must be `"zephyr,lq-adc-source"` |
| `adc` | phandle | Yes | - | Reference to ADC device |
| `channel` | int | Yes | - | ADC channel number |
| `output-queue` | phandle | Yes | - | Destination queue |
| `poll-interval-ms` | int | No | `100` | Polling interval in milliseconds |
| `averaging` | int | No | `1` | Number of samples to average |

**Child Nodes (Ranges)**:
- `min` (int, required): Minimum value (inclusive)
- `max` (int, required): Maximum value (inclusive)
- `status` (int, required): Status code (`LQ_OK`, `LQ_DEGRADED`, etc.)

### SPI Source Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `compatible` | string | Yes | - | Must be `"zephyr,lq-spi-source"` |
| `spi` | phandle | Yes | - | Reference to SPI bus |
| `reg` | int | Yes | - | Register/chip select |
| `output-queue` | phandle | Yes | - | Destination queue |
| `poll-interval-ms` | int | No | `100` | Polling interval |
| `read-length` | int | No | `2` | Bytes to read per transaction |

**Child Nodes (Expected Values)**:
- `value` (int, required): Expected value
- `status` (int, required): Status when value matches

### Merge/Voter Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `compatible` | string | Yes | - | Must be `"zephyr,lq-merge-voter"` |
| `input-queues` | phandles | Yes | - | Array of input queues |
| `output-queue` | phandle | Yes | - | Destination queue |
| `voting-method` | string | No | `"median"` | `"median"`, `"average"`, `"min"`, `"max"`, `"majority"` |
| `tolerance` | int | No | `0` | Max deviation between inputs |
| `status-if-violation` | int | No | `LQ_OUT_OF_RANGE` | Status when tolerance exceeded |
| `timeout-ms` | int | No | `1000` | Max age of cached values |

**Child Node (Range)**:
- `min` (int, required): Minimum acceptable value
- `max` (int, required): Maximum acceptable value
- `status-if-violation` (int, required): Status if out of range

### Dual-Inverted Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `compatible` | string | Yes | - | Must be `"zephyr,lq-dual-inverted"` |
| `gpio-normal` | phandle | Yes | - | Normal signal GPIO |
| `gpio-inverted` | phandle | Yes | - | Inverted signal GPIO |
| `output-queue` | phandle | Yes | - | Destination queue |
| `poll-interval-ms` | int | No | `50` | Polling interval |
| `debounce-ms` | int | No | `10` | Debounce time |
| `error-on-both-high` | boolean | No | `true` | Error when both high |
| `error-on-both-low` | boolean | No | `true` | Error when both low |
| `error-status` | int | No | `LQ_ERROR` | Error state status |
| `ok-status` | int | No | `LQ_OK` | Normal state status |

## Best Practices

### 1. Range Ordering

Define ranges from **most specific to most general**. First match wins.

```dts
/* Good - specific first */
valid {
    min = <1000>;
    max = <2000>;
    status = <LQ_OK>;
};

degraded {
    min = <900>;
    max = <2100>;
    status = <LQ_DEGRADED>;
};

any {
    min = <0>;
    max = <4095>;
    status = <LQ_OUT_OF_RANGE>;
};

/* Bad - general first (would match everything) */
any {
    min = <0>;
    max = <4095>;
    status = <LQ_OUT_OF_RANGE>;
};

valid {
    min = <1000>;  /* Never reached! */
    max = <2000>;
    status = <LQ_OK>;
};
```

### 2. Queue Sizing

Size queues based on: **capacity > poll_rate × consumer_latency**

```dts
/* Producer: 100Hz (10ms interval) */
/* Consumer: processes in <50ms */
/* Capacity needed: 100 Hz × 0.05s = 5 items minimum */

my_queue: lq-queue@0 {
    capacity = <16>;  /* 3× safety margin */
    drop-policy = "drop-oldest";
};
```

### 3. Voting Tolerance

Set tolerance appropriate for sensor resolution:

```dts
/* 12-bit ADC, 0-5V → 1.22mV per LSB */
/* Allow ±10 LSB tolerance = ±12.2mV */

voter: lq-merge@0 {
    tolerance = <10>;
    voting-method = "median";
};
```

### 4. Poll Intervals

Balance responsiveness vs. CPU overhead:

- Safety-critical: 10-50ms
- Normal operation: 100-500ms
- Slow monitoring: 1000-5000ms

### 5. Averaging

Reduce noise without filtering:

```dts
adc_noisy: lq-source@0 {
    averaging = <8>;  /* Average 8 samples */
    poll-interval-ms = <100>;
    /* Effective update rate: 100ms × 8 = 800ms */
};
```

## Common Patterns

### Pattern 1: Dual Redundant Sensors

```dts
/ {
    q1: lq-queue@0 { capacity = <16>; };
    q2: lq-queue@1 { capacity = <16>; };
    q_out: lq-queue@2 { capacity = <32>; };

    sensor1: lq-source@0 {
        compatible = "zephyr,lq-adc-source";
        adc = <&adc1>;
        channel = <0>;
        output-queue = <&q1>;
    };

    sensor2: lq-source@1 {
        compatible = "zephyr,lq-adc-source";
        adc = <&adc2>;
        channel = <0>;
        output-queue = <&q2>;
    };

    voter: lq-merge@0 {
        compatible = "zephyr,lq-merge-voter";
        input-queues = <&q1 &q2>;
        output-queue = <&q_out>;
        voting-method = "median";
        tolerance = <50>;
    };
};
```

### Pattern 2: ADC + SPI Cross-Check

```dts
/ {
    q_adc: lq-queue@0 { capacity = <16>; };
    q_spi: lq-queue@1 { capacity = <16>; };
    q_merged: lq-queue@2 { capacity = <32>; };

    adc_sensor: lq-source@0 {
        compatible = "zephyr,lq-adc-source";
        adc = <&adc1>;
        channel = <2>;
        output-queue = <&q_adc>;
        averaging = <4>;
    };

    spi_sensor: lq-source@1 {
        compatible = "zephyr,lq-spi-source";
        spi = <&spi2>;
        reg = <0x10>;
        output-queue = <&q_spi>;
    };

    cross_check: lq-merge@0 {
        compatible = "zephyr,lq-merge-voter";
        input-queues = <&q_adc &q_spi>;
        output-queue = <&q_merged>;
        tolerance = <100>;
        status-if-violation = <LQ_INCONSISTENT>;
    };
};
```

### Pattern 3: Safety Switch with Dual-Inverted

```dts
/ {
    q_safety: lq-queue@0 {
        capacity = <4>;
        priority = <100>;  /* High priority */
    };

    estop_switch: lq-dual-inverted@0 {
        compatible = "zephyr,lq-dual-inverted";
        gpio-normal = <&gpio0 4 GPIO_ACTIVE_HIGH>;
        gpio-inverted = <&gpio0 5 GPIO_ACTIVE_HIGH>;
        output-queue = <&q_safety>;
        debounce-ms = <5>;
        poll-interval-ms = <10>;
    };
};
```

## Troubleshooting

### Queue Overflows

**Symptom**: `items_dropped` counter increasing

**Solutions**:
1. Increase queue `capacity`
2. Speed up consumer
3. Reduce producer `poll-interval-ms`
4. Change `drop-policy` to `"block"`

### Inconsistent Merge Results

**Symptom**: `LQ_INCONSISTENT` status on merged output

**Solutions**:
1. Increase `tolerance`
2. Check sensor calibration
3. Add `averaging` to reduce noise
4. Verify sensors reading same physical quantity

### High CPU Usage

**Symptom**: System sluggish, high CPU load

**Solutions**:
1. Increase `poll-interval-ms`
2. Reduce `averaging` count
3. Lower queue `priority`
4. Disable unused sources

## Examples

See full examples in:
- [dts/examples/layered-queue-example.dts](../dts/examples/layered-queue-example.dts)
- [dts/examples/automotive-brake-example.dts](../dts/examples/automotive-brake-example.dts)
