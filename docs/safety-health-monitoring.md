# Safety-Critical Peripheral Health Monitoring

## Problem Statement

In safety-critical systems (automotive, medical, industrial), **you must know when peripherals fail to initialize or become unresponsive**. A missing brake sensor or failed steering encoder could be fatal.

### The Challenge

When a Zephyr driver initialization fails:
```c
if (!device_is_ready(config->spi_dev)) {
    LOG_ERR("SPI device not ready");
    return -ENODEV;  // ← How does the fault monitor know?
}
```

The question: **How do we propagate this critical failure to the safety system?**

## Solution: System Health Monitoring

A centralized health registry that all hardware drivers report to, combined with signal-based monitoring.

### Architecture

```
Hardware Driver Init → Health Registry → Health Signal → Fault Monitor → Fail-Safe Action
        ↓                    ↓                ↓               ↓              ↓
   ADC/SPI/etc          Track Status     Publish to      Detect        Disable
   device_ready()       (OK/FAILED)      Signal Bus     Unsafe State   Actuators
```

## Implementation

### 1. Hardware Drivers Register Health Status

Every hardware driver reports its initialization status:

```c
static int lq_hw_spi_init(const struct device *dev)
{
    struct lq_hw_spi_data *data = dev->data;
    const struct lq_hw_spi_config *config = dev->config;
    
    /* Check if SPI bus is ready */
    if (!device_is_ready(config->spi_dev)) {
        LOG_ERR("SPI device not ready - CRITICAL FAILURE");
        
        /* ✅ REGISTER FAILURE - Safety system will know */
        data->health_index = lq_health_register(
            dev->name,
            LQ_HEALTH_INIT_FAILED,  // Critical status
            -ENODEV                  // Error code
        );
        
        return -ENODEV;
    }
    
    /* ... successful initialization ... */
    
    /* ✅ REGISTER SUCCESS */
    data->health_index = lq_health_register(
        dev->name,
        LQ_HEALTH_OK,
        0
    );
    
    return 0;
}
```

### 2. Runtime Health Updates

Drivers update health during operation:

```c
static void lq_hw_spi_sample_work_handler(struct k_work *work)
{
    int ret = spi_transceive(data->spi_dev, &data->spi_cfg, &tx, &rx);
    
    if (ret == 0) {
        lq_hw_push(data->hw_source, value);
        
        /* ✅ Report healthy operation */
        lq_health_update(data->health_index, LQ_HEALTH_OK, 0);
        
    } else {
        LOG_ERR("SPI read failed: %d", ret);
        
        /* ✅ Report degraded operation */
        lq_health_update(data->health_index, LQ_HEALTH_DEGRADED, ret);
    }
}
```

### 3. System Health Published as Signal

The health monitor publishes aggregated status:

```dts
/ {
    system_health: lq-system-health {
        compatible = "lq,system-health";
        update-period-ms = <50>;  /* 20Hz update rate */
    };
};
```

Health signal encoding:
- **Bits 0-7**: Number of registered devices
- **Bits 8-15**: Number of critical failures
- **Bit 16**: System safe flag (1=safe, 0=**UNSAFE**)

### 4. Fault Monitor Detects Unsafe State

```dts
/ {
    system_safe_monitor: lq-fault {
        compatible = "lq,fault-monitor";
        input = <&system_health>;
        
        /* Threshold: bit 16 must be set (value >= 65536) */
        threshold-low = <65536>;
        
        /* No hysteresis or debounce - immediate detection */
        hysteresis = <0>;
        debounce-ms = <0>;
    };
};
```

When ANY peripheral has `LQ_HEALTH_INIT_FAILED` or `LQ_HEALTH_FAILED`:
- Bit 16 clears
- Health signal value < 65536
- Fault monitor triggers
- Fail-safe actions activate

## Application Integration

### Basic Safety Check

```c
void main(void) {
    lq_engine_init();
    
    /* ✅ Check if all peripherals initialized successfully */
    if (!lq_health_is_system_safe()) {
        LOG_ERR("CRITICAL: Peripheral initialization failed!");
        LOG_ERR("Entering fail-safe mode - actuators disabled");
        
        /* Log failed devices */
        const struct lq_health_registry *health = lq_health_get_status();
        for (int i = 0; i < health->num_devices; i++) {
            const struct lq_health_entry *entry = &health->entries[i];
            if (entry->status == LQ_HEALTH_INIT_FAILED) {
                LOG_ERR("  Failed: %s (errno %d)",
                        entry->device_name,
                        entry->error_code);
            }
        }
        
        /* Stay in safe mode */
        while (1) {
            k_msleep(1000);
        }
    }
    
    LOG_INF("All peripherals OK - entering operational mode");
    
    while (1) {
        lq_engine_process();
        k_msleep(10);
    }
}
```

### Fail-Safe Output Control

Use verified output to gate actuators:

```dts
/ {
    /* Only enable motor if system is safe */
    motor_enable: lq-verified-output {
        compatible = "lq,verified-output";
        primary = <&system_safe_monitor>;
        secondary = <&system_safe_monitor>;
        tolerance = <0>;
    };
};
```

Motor will only receive enable signal if:
1. All sensors initialized successfully
2. No runtime failures detected
3. Health monitor confirms system safe

## Health Status Codes

```c
enum lq_health_status {
    LQ_HEALTH_UNKNOWN = 0,       // Not yet initialized
    LQ_HEALTH_OK = 1,            // ✅ Fully operational
    LQ_HEALTH_INIT_FAILED = 2,   // 🔴 Init failed - CRITICAL
    LQ_HEALTH_DEGRADED = 3,      // ⚠️ Working but with errors
    LQ_HEALTH_STALE = 4,         // ⚠️ No new data (timeout)
    LQ_HEALTH_FAILED = 5,        // 🔴 Runtime failure - CRITICAL
};
```

**Critical statuses** (make system unsafe):
- `LQ_HEALTH_INIT_FAILED` - Device didn't initialize
- `LQ_HEALTH_FAILED` - Device stopped working

**Warning statuses** (degraded but operational):
- `LQ_HEALTH_DEGRADED` - Intermittent errors
- `LQ_HEALTH_STALE` - Data timeout

## API Reference

### Registration (in driver init)

```c
int lq_health_register(const char *name, 
                        enum lq_health_status status,
                        int error_code);
```

Returns: Health registry index (store in driver data)

### Update (during operation)

```c
void lq_health_update(int index, 
                       enum lq_health_status status,
                       int error_code);
```

### Query (in application)

```c
/* Quick safety check */
bool lq_health_is_system_safe(void);

/* Detailed status */
const struct lq_health_registry* lq_health_get_status(void);

/* Signal value (for devicetree monitoring) */
uint32_t lq_health_as_signal(void);
```

## Example: Automotive Brake System

```dts
/ {
    /* Health monitor */
    system_health: lq-system-health {
        compatible = "lq,system-health";
        update-period-ms = <50>;
    };
    
    /* Brake pressure sensors (dual redundant) */
    brake_primary: lq-hw-adc {
        compatible = "lq,hw-adc-input";
        io-channels = <&adc0 0>;
        /* If ADC0 fails, health monitor knows immediately */
    };
    
    brake_secondary: lq-hw-adc {
        compatible = "lq,hw-adc-input";
        io-channels = <&adc1 0>;
        /* If ADC1 fails, health monitor knows immediately */
    };
    
    /* System safety monitor */
    brake_system_safe: lq-fault {
        compatible = "lq,fault-monitor";
        input = <&system_health>;
        threshold-low = <65536>;  /* Bit 16 = system safe */
        debounce-ms = <0>;        /* No debounce for init failures */
    };
    
    /* Brake actuator - only enable if system safe */
    brake_actuator: lq-verified-output {
        compatible = "lq,verified-output";
        primary = <&brake_system_safe>;
        secondary = <&brake_system_safe>;
        /* Actuator disabled if ANY sensor failed to init */
    };
};
```

## Configuration

### Enable Health Monitoring

```ini
# prj.conf
CONFIG_LQ_DRIVER=y
CONFIG_LQ_SYSTEM_HEALTH=y
CONFIG_LQ_FAULT_MONITOR=y
CONFIG_LQ_VERIFIED_OUTPUT=y
```

### Initialization Priority

Health monitor must initialize **before** any hardware drivers:

```ini
CONFIG_LQ_SYSTEM_HEALTH_INIT_PRIORITY=10   # Very early
CONFIG_LQ_HW_ADC_INIT_PRIORITY=80          # After health monitor
CONFIG_LQ_HW_SPI_INIT_PRIORITY=80          # After health monitor
```

## Benefits

### 1. **Immediate Failure Detection**
Init failures detected before entering operational mode.

### 2. **Signal-Based Monitoring**
Use standard fault monitors - no special application code needed.

### 3. **Automatic Fail-Safe**
Verified outputs automatically disable when sensors fail.

### 4. **Diagnostic Logging**
Detailed failure information for troubleshooting.

### 5. **Runtime Health Tracking**
Detect intermittent failures and degraded operation.

## Testing

### Simulate Init Failure

Force SPI device to fail:

```dts
&spi0 {
    status = "disabled";  /* SPI bus disabled */
};

/ {
    steering_sensor: lq-hw-spi {
        compatible = "lq,hw-spi-input";
        /* Will fail: spi0 not ready */
    };
};
```

Expected behavior:
1. `lq_hw_spi_init()` returns `-ENODEV`
2. Health monitor registers `LQ_HEALTH_INIT_FAILED`
3. System health bit 16 clears
4. Fault monitor triggers
5. Verified outputs disable
6. Application detects unsafe state

### Verify Health Status

```c
void test_health_monitoring(void)
{
    const struct lq_health_registry *health = lq_health_get_status();
    
    printf("System safe: %s\n", 
           lq_health_is_system_safe() ? "YES" : "NO");
    printf("Devices: %u, Failures: %u\n",
           health->num_devices,
           health->critical_failures);
    
    for (int i = 0; i < health->num_devices; i++) {
        const struct lq_health_entry *e = &health->entries[i];
        printf("  %s: status=%d, errors=%u\n",
               e->device_name,
               e->status,
               e->consecutive_errors);
    }
}
```

## Related Files

- [include/lq_system_health.h](../include/lq_system_health.h) - API definitions
- [src/drivers/lq_system_health.c](../src/drivers/lq_system_health.c) - Core implementation
- [zephyr/drivers/lq_system_health.c](drivers/lq_system_health.c) - Zephyr driver
- [zephyr/drivers/lq_hw_spi.c](drivers/lq_hw_spi.c) - Example integration
- [zephyr/safety-critical-example.overlay](safety-critical-example.overlay) - Complete example

## Standards Compliance

This implementation supports safety standards requiring:

- **ISO 26262** (Automotive): Systematic fault detection
- **IEC 61508** (Industrial): Safe failure fraction
- **DO-178C** (Aviation): Hardware monitoring
- **IEC 62304** (Medical): Risk mitigation

By detecting all peripheral initialization failures and providing fail-safe mechanisms.
