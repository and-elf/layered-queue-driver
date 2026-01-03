# Automotive Engine Monitor Sample

This sample demonstrates **complete auto-generation** from devicetree with **real sensor driver integration**.

## Two DTS Files

### 1. `app.dts` - Simple ADC/SPI Example
Basic example using raw ADC and SPI inputs (original version).

### 2. `app_sensors.dts` - Real Sensor Drivers ⭐
**Recommended:** Uses actual Zephyr sensor drivers:
- **TMP117** - High-precision I2C temperature (±0.1°C)
- **BME280** - Environmental sensor (temp, humidity, pressure)
- **LSM6DSL** - 6-axis IMU for vibration monitoring
- **ICP10125** - High-accuracy barometric pressure

## Device Initialization Priority

The system ensures proper initialization order:

```
Priority 50-59: I2C/SPI/CAN bus drivers
    ↓
Priority 60-69: Sensor drivers (TMP117, BME280, LSM6DSL, ICP10125)
    ↓
Priority 80:    Hardware input layer (ISR/trigger handlers)
    ↓
Priority 85:    Engine processing layer
    ↓
Priority 90+:   Application tasks
```

This is configured via `init-priority` in DTS:

```dts
tmp117_engine: tmp117@48 {
    compatible = "ti,tmp117";
    /* Driver init priority 70 (from sensor driver) */
};

temp_input: lq-hw-sensor-input@0 {
    compatible = "lq,hw-sensor-input";
    sensor-device = <&tmp117_engine>;
    init-priority = <80>;  /* After sensor driver */
};

engine: lq-engine {
    compatible = "lq,engine";
    init-priority = <85>;  /* After all inputs */
};
```

## What's Auto-Generated

From the `app.dts` file, the following code is automatically generated:

### 1. Engine Struct Initialization
```c
static struct lq_engine engine = LQ_ENGINE_DT_INIT(DT_NODELABEL(engine));
```
This single line creates a fully configured engine with:
- All signal storage allocated
- All merge contexts configured (voting methods, tolerances, signal IDs)
- All cyclic output contexts configured (periods, deadlines, target IDs)

### 2. ISR Handlers
```c
LQ_FOREACH_HW_ADC_INPUT(LQ_GEN_ADC_ISR_HANDLER)
LQ_FOREACH_HW_SPI_INPUT(LQ_GEN_SPI_ISR_HANDLER)
LQ_FOREACH_HW_SENSOR_INPUT(LQ_GEN_SENSOR_TRIGGER_HANDLER)
```
Generates interrupt/trigger handlers for each input:
- **ADC inputs**: Read ADC values on interrupt
- **SPI inputs**: Read SPI sensor data on GPIO trigger
- **Sensor inputs**: Fetch from Zephyr sensor API on data-ready

All handlers call `lq_hw_push()` to insert into ISR-safe ringbuffer.

### 3. Signal IDs
All signal IDs are compile-time constants from DTS:
- `rpm_adc` → signal 0
- `rpm_spi` → signal 1
- `temp_adc` → signal 2
- `oil_adc` → signal 3
- `rpm_merge` → signal 10 (merged result)

## DTS Configuration

### Using Real Sensor Drivers

```dts
/* Reference actual sensor device */
temp_tmp117: lq-hw-sensor-input@0 {
    compatible = "lq,hw-sensor-input";
    signal-id = <2>;
    sensor-device = <&tmp117_engine>;  /* Phandle to TMP117 */
    sensor-channel = <0>;              /* SENSOR_CHAN_AMBIENT_TEMP */
    trigger-type = "data-ready";       /* Use sensor's interrupt */
    scale-factor = <100>;              /* 0.01°C precision */
    stale-us = <100000>;
    init-priority = <80>;              /* After TMP117 driver (70) */
};

/* The actual sensor driver node (in board DTS) */
&i2c0 {
    tmp117_engine: tmp117@48 {
        compatible = "ti,tmp117";
        reg = <0x48>;
        status = "okay";
    };
};
```

### Supported Sensor Types

**Temperature:**
- `ti,tmp117` - High-precision (±0.1°C)
- `bosch,bme280` - Environmental sensor
- `st,lps22hh` - Pressure + temperature

**Pressure:**
- `invensense,icp10125` - High accuracy barometric
- `bosch,bme280` - Environmental
- `st,lps22hh` - MEMS pressure

**Motion/Vibration:**
- `st,lsm6dsl` - 6-axis IMU
- `st,lis2dh` - 3-axis accelerometer
- `invensense,mpu6050` - 6-axis motion

**Any Zephyr sensor driver** with the sensor API works!

### Legacy Raw ADC/SPI

For sensors without Zephyr drivers:

```dts
lq-hw-adc-input@0 {
    compatible = "lq,hw-adc-input";
    signal-id = <0>;
    adc-channel = <0>;
    stale-us = <5000>;
}

lq-hw-spi-input@0 {
    compatible = "lq,hw-spi-input";
    signal-id = <1>;
    spi-device = <&spi0>;
    num-bytes = <2>;
    signed;
}
```

### Mid-Level Processing

Merge multiple sensors with voting:

```dts
lq-mid-merge@0 {
    compatible = "lq,mid-merge";
    output-signal-id = <10>;
    input-signal-ids = <0 1>;
    voting-method = "median";
    tolerance = <50>;         /* Max 50 RPM deviation */
}
```

### Cyclic Outputs

Auto-scheduled periodic transmissions:

```dts
lq-cyclic-output@0 {
    compatible = "lq,cyclic-output";
    source-signal-id = <10>;
    output-type = "j1939";
    target-id = <0xFEF1>;     /* Engine Speed PGN */
    period-us = <100000>;     /* 10Hz */
}
```

## Adding New Sensors

### Option 1: Use Existing Zephyr Driver

1. Add sensor device to your board's DTS:
```dts
&i2c0 {
    my_sensor: bme280@76 {
        compatible = "bosch,bme280";
        reg = <0x76>;
        status = "okay";
    };
};
```

2. Add sensor input to app DTS:
```dts
new_input: lq-hw-sensor-input@4 {
    compatible = "lq,hw-sensor-input";
    signal-id = <6>;
    sensor-device = <&my_sensor>;
    sensor-channel = <0>;     /* SENSOR_CHAN_AMBIENT_TEMP */
    trigger-type = "data-ready";
    scale-factor = <100>;
    init-priority = <80>;
};
```

3. Add cyclic output:
```dts
can_new: lq-cyclic-output@4 {
    compatible = "lq,cyclic-output";
    source-signal-id = <6>;
    output-type = "j1939";
    target-id = <0xFEF3>;
    period-us = <500000>;     /* 2Hz */
}
```

4. **Rebuild** - everything is auto-generated!

### Option 2: Raw ADC/SPI (No Driver)

For custom sensors without Zephyr drivers, use `lq,hw-adc-input` or `lq,hw-spi-input`.

## Real-World Sensors in This Sample

| Sensor | Part Number | Interface | Application |
|--------|-------------|-----------|-------------|
| Temperature | TMP117 | I2C | Engine block temp (±0.1°C) |
| Environmental | BME280 | I2C | Coolant system monitor |
| Pressure | ICP10125 | I2C | Oil pressure (high accuracy) |
| Vibration | LSM6DSL | I2C + IRQ | Engine vibration analysis |
| RPM | Hall Effect | ADC | Primary tachometer |
| RPM | Encoder | ADC/Timer | Secondary tachometer |

All integrated with **zero boilerplate** - just DTS configuration!

## Building

```bash
# Using sensor version (recommended)
west build -b <your_board> samples/automotive -- -DDTS_FILE=app_sensors.dts

# Using simple ADC/SPI version
west build -b <your_board> samples/automotive

west flash
```

```dts
lq-hw-adc-input@0 {
    compatible = "lq,hw-adc-input";
    signal-id = <0>;
    adc-channel = <0>;
    stale-us = <5000>;
}

lq-mid-merge@0 {
    compatible = "lq,mid-merge";
    output-signal-id = <10>;
    input-signal-ids = <0 1>;
    voting-method = "median";
    tolerance = <50>;
}

lq-cyclic-output@0 {
    compatible = "lq,cyclic-output";
    source-signal-id = <10>;
    output-type = "j1939";
    target-id = <0xFEF1>;
    period-us = <100000>;  /* 10Hz */
}
```

## Building

```bash
west build -b <your_board> samples/automotive
west flash
```

## Adding New Sensors

To add a new sensor:

1. Add input node to `app.dts`:
```dts
boost_adc: lq-hw-adc-input@3 {
    compatible = "lq,hw-adc-input";
    signal-id = <4>;
    adc-channel = <3>;
    stale-us = <20000>;
}
```

2. Add cyclic output:
```dts
can_boost: lq-cyclic-output@3 {
    compatible = "lq,cyclic-output";
    source-signal-id = <4>;
    output-type = "j1939";
    target-id = <0xFEF2>;
    period-us = <500000>;  /* 2Hz */
}
```

3. Rebuild - ISR and engine config are automatically updated!

## Architecture

```
Hardware IRQ → Auto-generated ISR → lq_hw_push() → Ringbuffer
                                                        ↓
                                                   Engine Task
                                                        ↓
                                    Phase 1: Ingest (pull from ringbuffer)
                                    Phase 2: Staleness checks
                                    Phase 3: Merges (median voting)
                                    Phase 4: On-change outputs
                                    Phase 5: Cyclic outputs (deadline scheduling)
                                                        ↓
                                                  Output Events
                                                        ↓
                                            CAN/GPIO/UART drivers
```

All phases configured from DTS - no manual struct initialization!
