# Fault Monitoring and Safety Concept

## Overview

The layered queue driver provides **declarative fault monitoring with immediate wake callbacks** for safety-critical systems. When fault conditions are detected, the system immediately calls user-defined wake functions to enable rapid safety responses.

## Key Concepts

### Wake Functions - The Safety Concept

**Wake functions** are user-implemented callbacks that execute **immediately** when a fault is detected or cleared. This enables:

- **Immediate response** - No polling, action taken in the same cycle as detection
- **Deterministic behavior** - Predictable execution time and order
- **Safety compliance** - Meets requirements for ISO 26262 / IEC 61508
- **Declarative configuration** - Define in devicetree, implement in C

### Architecture

```
Sensor → Merge/Vote → Fault Monitor → Wake Function
                            ↓              ↓
                       Fault Signal    Safety Action
                            ↓
                       CAN Output
```

1. **Detection** - Fault monitor checks conditions (staleness, range, status)
2. **Wake** - Immediately calls user's wake function
3. **Signal** - Sets fault output signal for logging/diagnostics
4. **Output** - Fault flag transmitted via CAN

## Fault Monitor Node

Fault monitors are defined in devicetree with wake function callbacks:

```dts
sensor_fault: lq-fault-monitor@0 {
    compatible = "lq,fault-monitor";
    status = "okay";
    
    input-signal-id = <10>;           /* Signal to monitor */
    fault-output-signal-id = <20>;    /* Fault flag output */
    
    /* Enable fault detection modes */
    check-staleness;
    stale-timeout-us = <50000>;       /* 50ms timeout */
    
    check-range;
    min-value = <0>;
    max-value = <5000>;
    
    check-status;  /* Detect merge/voting failures */
    
    /* Wake function - called immediately on fault state change */
    wake-function = "sensor_fault_wake";
};
```

## Implementing Wake Functions

Wake functions have the signature:

```c
void function_name(uint8_t monitor_id, int32_t input_value, bool fault_detected);
```

**Parameters:**
- `monitor_id` - Index of the fault monitor (0-based)
- `input_value` - Current value of the monitored signal
- `fault_detected` - `true` when fault occurs, `false` when cleared

**Example Implementation:**

```c
#include "lq_generated.h"

void sensor_fault_wake(uint8_t monitor_id, int32_t input_value, bool fault_detected)
{
    if (fault_detected) {
        /* Fault just detected - take immediate action */
        switch_to_backup_sensor();
        enable_limp_mode();
        log_fault_event(monitor_id, input_value);
    } else {
        /* Fault cleared - restore normal operation */
        restore_normal_mode();
    }
}
```

### Code Generation

The generator creates:

1. **Header declaration** in `lq_generated.h`:
   ```c
   void sensor_fault_wake(uint8_t monitor_id, int32_t input_value, bool fault_detected);
   ```

2. **Weak stub** in `lq_generated.c`:
   ```c
   __attribute__((weak))
   void sensor_fault_wake(uint8_t monitor_id, int32_t input_value, bool fault_detected) {
       /* Default: no action */
   }
   ```

3. **User overrides** the weak stub with actual implementation

## Fault Detection Modes

### 1. Staleness Detection
Detects when a signal hasn't updated within the specified timeout:

```dts
check-staleness;
stale-timeout-us = <100000>;  /* 100ms */
```

Use case: Detect sensor communication failures

### 2. Range Checking
Detects when signal values exceed acceptable bounds:

```dts
check-range;
min-value = <-40>;   /* -40°C */
max-value = <125>;   /* 125°C */
```

Use case: Detect sensor malfunction or out-of-spec conditions

### 3. Status Checking
Detects fault conditions from upstream processing (e.g., voting inconsistencies):

```dts
check-status;
```

Use case: Detect when redundant sensors disagree beyond tolerance

## Complete Example

```dts
/ {
    /* Redundant temperature sensors */
    temp_a: lq-hw-adc-input@0 {
        signal-id = <0>;
        stale-us = <10000>;
    };
    
    temp_b: lq-hw-adc-input@1 {
        signal-id = <1>;
        stale-us = <10000>;
    };
    
    /* Merge with median voting */
    temp_merged: lq-mid-merge@0 {
        output-signal-id = <10>;
        input-signal-ids = <0 1>;
        voting-method = "median";
        tolerance = <5>;  /* 0.5°C tolerance */
    };
    
    /* Monitor for sensor faults */
    temp_fault: lq-fault-monitor@0 {
        input-signal-id = <10>;
        fault-output-signal-id = <20>;
        
        /* Detect stale data */
        check-staleness;
        stale-timeout-us = <50000>;
        
        /* Detect out-of-range */
        check-range;
        min-value = <-40>;
        max-value = <125>;
        
        /* Detect voting failures */
        check-status;
        
        /* Immediate wake callback */
        wake-function = "temperature_fault_wake";
    };
    
    /* Output fault flag on CAN */
    fault_output: lq-cyclic-j1939-output@0 {
        source-signal-id = <20>;
        target-id = <0xFECA>;  /* Diagnostic message PGN */
        period-us = <1000000>; /* 1 Hz */
    };
};
```

**User Implementation:**

```c
void temperature_fault_wake(uint8_t monitor_id, int32_t temp, bool fault)
{
    if (fault) {
        if (temp > 1200) {  /* > 120°C */
            emergency_shutdown();
        } else {
            reduce_power(50);  /* Limit to 50% */
            enable_cooling();
        }
    } else {
        restore_normal_operation();
    }
}
```

## Fault Output Signal

The fault output signal has two states:
- **0**: No fault detected (OK)
- **1**: Fault condition active

This signal can be:
- Transmitted via CAN/J1939 for diagnostics
- Used by application logic for safety actions
- Combined with other fault signals using merge nodes

## Processing Order

Fault monitors run after merges in the engine processing pipeline:

1. **Ingest events** - New sensor data arrives
2. **Apply staleness** - Mark old signals as stale
3. **Process merges** - Run voting algorithms
4. **Process fault monitors** ← Fault detection happens here
5. **Process outputs** - Generate CAN messages

This ensures fault monitors can detect both individual sensor failures and merge/voting inconsistencies.

## Safety Applications

### Example 1: Engine Protection with Wake Functions

```dts
/* Critical parameter monitors */
temp_monitor: lq-fault-monitor@0 {
    input-signal-id = <TEMP_MERGED>;
    fault-output-signal-id = <TEMP_FAULT>;
    check-range;
    max-value = <115>;  /* Overheat threshold */
    wake-function = "overheat_wake";
};

oil_monitor: lq-fault-monitor@1 {
    input-signal-id = <OIL_PRESSURE>;
    fault-output-signal-id = <OIL_FAULT>;
    check-range;
    min-value = <20>;  /* Low pressure threshold */
    wake-function = "low_oil_wake";
};
```

**Safety handlers:**

```c
void overheat_wake(uint8_t id, int32_t temp, bool fault) {
    if (fault) {
        reduce_engine_power(25);  /* Immediate 75% power cut */
        set_warning_lamp(OVERHEAT_WARNING);
        log_safety_event("Overheating detected");
    }
}

void low_oil_wake(uint8_t id, int32_t pressure, bool fault) {
    if (fault) {
        initiate_controlled_shutdown();
        set_warning_lamp(OIL_PRESSURE_WARNING);
        log_safety_event("Low oil pressure");
    }
}
```

### Example 2: Redundant Sensor Failure Detection

```dts
sensor_merge: lq-mid-merge@0 {
    output-signal-id = <10>;
    input-signal-ids = <0 1 2>;  /* Triple redundancy */
    voting-method = "median";
    tolerance = <10>;
};

merge_monitor: lq-fault-monitor@0 {
    input-signal-id = <10>;
    fault-output-signal-id = <20>;
    check-status;  /* Detect voting failures */
    wake-function = "sensor_disagree_wake";
};
```

**Handler:**

```c
void sensor_disagree_wake(uint8_t id, int32_t value, bool fault) {
    if (fault) {
        /* Sensors disagree beyond tolerance */
        switch_to_degraded_mode();
        request_sensor_diagnostics();
    }
}
```

## Benefits

✅ **Immediate Action** - Wake functions execute in same cycle as fault detection  
✅ **Declarative** - Define fault conditions in devicetree, not C code  
✅ **Composable** - Combine multiple fault conditions per monitor  
✅ **Testable** - HIL tests can inject faults and verify wake function calls  
✅ **Deterministic** - Fixed execution time, no dynamic allocation  
✅ **Safety-Ready** - Structured approach for ISO 26262 / IEC 61508  
✅ **Weak Linking** - Default stubs allow gradual implementation  
✅ **Type-Safe** - Generated declarations ensure correct signatures

## Wake Function Best Practices

### DO:
- Keep wake functions **fast and deterministic**
- Use wake functions for **immediate safety-critical actions**
- Log fault events for diagnostics
- Set hardware to safe states (disable outputs, reduce power)
- Use fault output signals for CAN diagnostics

### DON'T:
- Don't block or wait in wake functions
- Don't use dynamic allocation
- Don't perform complex calculations
- Don't assume wake is called only once per fault
- Don't ignore the `fault_detected` parameter (handle both edges)

### Example Pattern:

```c
void critical_fault_wake(uint8_t id, int32_t value, bool fault)
{
    if (fault) {
        /* FAULT DETECTED - immediate action */
        disable_dangerous_outputs();
        set_safe_defaults();
        increment_fault_counter();
        timestamp_fault_event();
    } else {
        /* FAULT CLEARED - cautious recovery */
        if (safe_to_recover()) {
            restore_normal_operation();
            clear_fault_counter();
        }
    }
}
```
